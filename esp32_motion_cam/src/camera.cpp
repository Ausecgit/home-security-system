#include "camera.h"
#include "config.h"

static bool initialized = false;

bool camera_init() {
  camera_config_t cfg;
  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.ledc_timer = LEDC_TIMER_0;
  cfg.pin_pwdn = CAM_PIN_PWDN;
  cfg.pin_reset = CAM_PIN_RESET;
  cfg.pin_xclk = CAM_PIN_XCLK;
  cfg.pin_sccb_sda = CAM_PIN_SIOD;
  cfg.pin_sccb_scl = CAM_PIN_SIOC;
  cfg.pin_d7 = CAM_PIN_Y9;
  cfg.pin_d6 = CAM_PIN_Y8;
  cfg.pin_d5 = CAM_PIN_Y7;
  cfg.pin_d4 = CAM_PIN_Y6;
  cfg.pin_d3 = CAM_PIN_Y5;
  cfg.pin_d2 = CAM_PIN_Y4;
  cfg.pin_d1 = CAM_PIN_Y3;
  cfg.pin_d0 = CAM_PIN_Y2;
  cfg.pin_vsync = CAM_PIN_VSYNC;
  cfg.pin_href = CAM_PIN_HREF;
  cfg.pin_pclk = CAM_PIN_PCLK;
  cfg.xclk_freq_hz = 20000000;
  cfg.frame_size = DET_FRAMESIZE;
  cfg.pixel_format = PIXFORMAT_YUV422;
  cfg.grab_mode = CAMERA_GRAB_LATEST;
  cfg.fb_location = CAMERA_FB_IN_PSRAM;
  cfg.fb_count = 2;
  cfg.jpeg_quality = 12;

  esp_err_t err = esp_camera_init(&cfg);
  initialized = (err == ESP_OK);
  return initialized;
}

camera_fb_t* camera_grab() {
  if (!initialized) return nullptr;
  return esp_camera_fb_get();
}
