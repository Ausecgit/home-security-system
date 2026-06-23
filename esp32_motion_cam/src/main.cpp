#include <Arduino.h>
#include <WiFi.h>
#include "esp_camera.h"
#include "img_converters.h"
#include "esp_heap_caps.h"
#include "config.h"
#include "camera.h"
#include "background.h"
#include "blob.h"
#include "net.h"
#include "denoise.h"

static int detW = 0, detH = 0;
static uint8_t* planeY = nullptr;
static uint8_t* planeU = nullptr;
static uint8_t* planeV = nullptr;
static uint8_t* blurY = nullptr;
static uint8_t* blurU = nullptr;
static uint8_t* blurV = nullptr;
static uint8_t* mask = nullptr;
static uint8_t* eroded = nullptr;
static uint8_t* prevMask = nullptr;
static bool aeLocked = false;
static int consecutiveHits = 0;

static uint32_t frameId = 1;
static unsigned long lastBgSample = 0;
static unsigned long motionUntil = 0;
static bool frozen = false;
static bool camOK = false;
static bool netOK = false;
static unsigned long lastHeartbeat = 0;

#define LED_PIN 48

static void led_blink(int n, int ms = 100) {
  pinMode(LED_PIN, OUTPUT);
  for (int i = 0; i < n; i++) {
    digitalWrite(LED_PIN, HIGH); delay(ms);
    digitalWrite(LED_PIN, LOW); delay(ms);
  }
  delay(300);
}

static void publish_heartbeat(const char* stage) {
  net_publish_status(CAM_ID, stage, frameId);
}

static void split_yuyv(const uint8_t* buf, int w, int h,
                       uint8_t* Y, uint8_t* U, uint8_t* V) {
  size_t i = 0;
  for (int row = 0; row < h; row++) {
    uint8_t* yrow = Y + (size_t)row * w;
    uint8_t* urow = U + (size_t)row * w;
    uint8_t* vrow = V + (size_t)row * w;
    for (int x = 0; x < w; x += 2) {
      uint8_t y0 = buf[i++];
      uint8_t u  = buf[i++];
      uint8_t y1 = buf[i++];
      uint8_t v  = buf[i++];
      yrow[x]     = y0;
      yrow[x + 1] = y1;
      urow[x]     = u;
      urow[x + 1] = u;
      vrow[x]     = v;
      vrow[x + 1] = v;
    }
  }
}

static void set_framesize(framesize_t fs) {
  sensor_t* s = esp_camera_sensor_get();
  if (s) s->set_framesize(s, fs);
}

static void lock_ae_awb() {
  sensor_t* s = esp_camera_sensor_get();
  if (!s) return;
  s->set_exposure_ctrl(s, 0);
  s->set_whitebal(s, 0);
  s->set_gain_ctrl(s, 0);
  aeLocked = true;
}

static bool capture_and_send_still(uint32_t fid) {
  camera_fb_t* fb = camera_grab();
  if (!fb) {
    publish_heartbeat("still_nofb");
    return false;
  }

  uint8_t* jpg = nullptr;
  size_t jpg_len = 0;
  esp_err_t e = fmt2jpg(fb->buf, fb->len, fb->width, fb->height,
                        fb->format, 12, &jpg, &jpg_len);
  esp_camera_fb_return(fb);

  if (e != ESP_OK) {
    char msg[64];
    snprintf(msg, sizeof(msg), "still_fmt_warn=%d len=%u", (int)e, (unsigned)jpg_len);
    publish_heartbeat(msg);
  }
  if (!jpg || jpg_len == 0) {
    publish_heartbeat("still_no_output");
    if (jpg) free(jpg);
    return false;
  }

  bool ok = net_upload_still(jpg, jpg_len, fid);
  free(jpg);

  if (!ok) {
    publish_heartbeat("still_http_fail");
  }
  return ok;
}

void setup() {
  led_blink(1);
  Serial.begin(115200);
  delay(300);
  Serial.println("boot");

  if (!camera_init()) { Serial.println("cam init fail"); led_blink(10, 500); camOK = false; }
  else {
    camOK = true;
    led_blink(2);
    set_framesize(DET_FRAMESIZE);
    delay(100);

    camera_fb_t* probe = camera_grab();
    if (!probe) { Serial.println("probe fail"); led_blink(9, 500); camOK = false; }
    else {
      detW = probe->width;
      detH = probe->height;
      esp_camera_fb_return(probe);
      Serial.printf("det %dx%d\n", detW, detH);
    }
  }

  if (camOK && detW > 0) {
    int p = detW * detH;
    planeY = (uint8_t*)heap_caps_malloc(p, MALLOC_CAP_SPIRAM);
    planeU = (uint8_t*)heap_caps_malloc(p, MALLOC_CAP_SPIRAM);
    planeV = (uint8_t*)heap_caps_malloc(p, MALLOC_CAP_SPIRAM);
    blurY  = (uint8_t*)heap_caps_malloc(p, MALLOC_CAP_SPIRAM);
    blurU  = (uint8_t*)heap_caps_malloc(p, MALLOC_CAP_SPIRAM);
    blurV  = (uint8_t*)heap_caps_malloc(p, MALLOC_CAP_SPIRAM);
    mask   = (uint8_t*)heap_caps_malloc(p, MALLOC_CAP_SPIRAM);
    eroded = (uint8_t*)heap_caps_malloc(p, MALLOC_CAP_SPIRAM);
    prevMask = (uint8_t*)heap_caps_malloc(p, MALLOC_CAP_SPIRAM);
    if (!planeY || !planeU || !planeV || !blurY || !blurU || !blurV ||
        !mask || !eroded || !prevMask) {
      Serial.println("plane alloc fail"); led_blink(8, 500); camOK = false;
    }

    if (camOK && !bg_init(detW, detH, BG_WINDOW_N)) { Serial.println("bg alloc fail"); led_blink(7, 500); camOK = false; }
    if (camOK && !blob_init(detW, detH)) { Serial.println("blob alloc fail"); led_blink(6, 500); camOK = false; }
    memset(prevMask, 0, p);
  }

  net_init();
  delay(2000);
  netOK = net_connected();
  led_blink(3);
  Serial.println("ready");
  publish_heartbeat("setup_done");
}

void loop() {
  net_loop();

  unsigned long now = millis();

  if ((long)(now - lastHeartbeat) >= 10000) {
    lastHeartbeat = now;
    publish_heartbeat("running");
  }

  if (frozen && (long)(now - motionUntil) > 0) {
    frozen = false;
  }

  if (!camOK) { delay(5000); return; }

  camera_fb_t* fb = camera_grab();
  if (!fb) { delay(DET_LOOP_DELAY_MS); return; }
  split_yuyv(fb->buf, fb->width, fb->height, planeY, planeU, planeV);
  esp_camera_fb_return(fb);

  // Lock AE/AWB after enough background frames collected
  if (!aeLocked && bg_count() >= AE_LOCK_AFTER_FRAMES) {
    lock_ae_awb();
    publish_heartbeat("ae_locked");
  }

  // Denoise: 3x3 blur each plane
#if DENOISE_BLUR
  blur_3x3(planeY, blurY, detW, detH);
  blur_3x3(planeU, blurU, detW, detH);
  blur_3x3(planeV, blurV, detW, detH);
#else
  memcpy(blurY, planeY, detW * detH);
  memcpy(blurU, planeU, detW * detH);
  memcpy(blurV, planeV, detW * detH);
#endif

  if (!frozen && (long)(now - lastBgSample) >= BG_INTERVAL_MS) {
    bg_push_frame(blurY, blurU, blurV);
    lastBgSample = now;
  }

  uint32_t fid = frameId++;

  if (frozen) {
    // During cooldown: skip detection, just update prevMask
    // so temporal persistence resets when cooldown ends
    memset(prevMask, 0, detW * detH);
    consecutiveHits = 0;
  } else if (bg_ready()) {
    bg_compute_mask(blurY, blurU, blurV, mask);

#if DENOISE_ERODE
    erode_mask(mask, eroded, detW, detH);
#else
    memcpy(eroded, mask, detW * detH);
#endif

    // Temporal persistence: AND with previous mask
    int persistentPixels = 0;
    for (int i = 0; i < detW * detH; i++) {
      if (eroded[i] && prevMask[i]) {
        eroded[i] = 1;
        persistentPixels++;
      } else {
        eroded[i] = 0;
      }
    }

    // Save mask for next frame
    memcpy(prevMask, mask, detW * detH);

    if (persistentPixels > 0) {
      BlobInfo b = blob_analyze(eroded, detW, detH, MIN_BLOB_PIXELS);
      if (b.found) {
        consecutiveHits++;
        if (consecutiveHits >= TEMPORAL_PERSISTENCE) {
          frozen = true;
          motionUntil = now + MOTION_COOLDOWN_MS;
          consecutiveHits = 0;
          net_publish_motion(CAM_ID, now, b.cx, b.cy, b.area,
                             b.classification, fid);
          capture_and_send_still(fid);
          Serial.printf("motion fid=%u area=%d (%d,%d) %s\n",
                        fid, b.area, b.cx, b.cy, b.classification);
        }
      } else {
        consecutiveHits = 0;
      }
    } else {
      consecutiveHits = 0;
    }
  } else if (fid % 10 == 0) {
    Serial.printf("warming up bg=%d/%d\n", bg_count(), BG_WINDOW_N);
  }

  delay(DET_LOOP_DELAY_MS);
}
