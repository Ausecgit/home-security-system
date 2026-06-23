#include "background.h"
#include "config.h"
#include "esp_heap_caps.h"
#include <cmath>
#include <cstring>

static int W = 0, H = 0, N = 0, P = 0;
static uint8_t* ringY = nullptr;
static uint8_t* ringU = nullptr;
static uint8_t* ringV = nullptr;
static uint16_t* sumY = nullptr;
static uint16_t* sumU = nullptr;
static uint16_t* sumV = nullptr;
static uint32_t* sqY = nullptr;
static uint32_t* sqU = nullptr;
static uint32_t* sqV = nullptr;
static int head = 0;
static int count = 0;

static uint8_t* alloc_chan8(int n) {
  return (uint8_t*)heap_caps_malloc((size_t)n, MALLOC_CAP_SPIRAM);
}
static uint16_t* alloc_chan16(int n) {
  return (uint16_t*)heap_caps_malloc((size_t)n * 2, MALLOC_CAP_SPIRAM);
}
static uint32_t* alloc_chan32(int n) {
  return (uint32_t*)heap_caps_malloc((size_t)n * 4, MALLOC_CAP_SPIRAM);
}

bool bg_init(int width, int height, int window_n) {
  W = width; H = height; N = window_n; P = W * H;
  ringY = alloc_chan8(N * P);
  ringU = alloc_chan8(N * P);
  ringV = alloc_chan8(N * P);
  sumY = alloc_chan16(P);
  sumU = alloc_chan16(P);
  sumV = alloc_chan16(P);
  sqY = alloc_chan32(P);
  sqU = alloc_chan32(P);
  sqV = alloc_chan32(P);
  if (!ringY || !ringU || !ringV || !sumY || !sumU || !sumV || !sqY || !sqU || !sqV) {
    bg_deinit();
    return false;
  }
  memset(sumY, 0, P * 2);
  memset(sumU, 0, P * 2);
  memset(sumV, 0, P * 2);
  memset(sqY, 0, P * 4);
  memset(sqU, 0, P * 4);
  memset(sqV, 0, P * 4);
  head = 0;
  count = 0;
  return true;
}

void bg_deinit() {
  free(ringY); free(ringU); free(ringV);
  free(sumY); free(sumU); free(sumV);
  free(sqY); free(sqU); free(sqV);
  ringY = ringU = ringV = nullptr;
  sumY = sumU = sumV = nullptr;
  sqY = sqU = sqV = nullptr;
  W = H = N = P = 0;
  head = count = 0;
}

static inline void chan_push(uint8_t* ring, uint16_t* sum, uint32_t* sq,
                             const uint8_t* in) {
  uint8_t* slot = ring + (size_t)head * P;
  if (count >= N) {
    uint8_t* old = slot;
    for (int i = 0; i < P; i++) {
      uint16_t o = old[i];
      sum[i] -= o;
      sq[i] -= (uint32_t)o * o;
    }
  }
  for (int i = 0; i < P; i++) {
    uint16_t v = in[i];
    slot[i] = (uint8_t)v;
    sum[i] += v;
    sq[i] += (uint32_t)v * v;
  }
}

void bg_push_frame(const uint8_t* y, const uint8_t* u, const uint8_t* v) {
  if (!y || !u || !v) return;
  chan_push(ringY, sumY, sqY, y);
  chan_push(ringU, sumU, sqU, u);
  chan_push(ringV, sumV, sqV, v);
  head = (head + 1) % N;
  if (count < N) count++;
}

void bg_compute_mask(const uint8_t* y, const uint8_t* u, const uint8_t* v,
                     uint8_t* mask_out) {
  if (count == 0) {
    memset(mask_out, 0, P);
    return;
  }
  float inv = 1.0f / (float)count;
  float zthr = Z_THRESHOLD;
  for (int i = 0; i < P; i++) {
    float my = (float)sumY[i] * inv;
    float vy = (float)sqY[i] * inv - my * my;
    if (vy < 1.0f) vy = 1.0f;
    float sd = sqrtf(vy);
    float dy = fabsf((float)y[i] - my) / sd;

    float mu = (float)sumU[i] * inv;
    float vu = (float)sqU[i] * inv - mu * mu;
    if (vu < 1.0f) vu = 1.0f;
    sd = sqrtf(vu);
    float du = fabsf((float)u[i] - mu) / sd;

    float mv = (float)sumV[i] * inv;
    float vv = (float)sqV[i] * inv - mv * mv;
    if (vv < 1.0f) vv = 1.0f;
    sd = sqrtf(vv);
    float dv = fabsf((float)v[i] - mv) / sd;

    mask_out[i] = (dy > zthr || du > zthr || dv > zthr) ? 1 : 0;
  }
}

bool bg_ready() { return count > 0; }
int bg_count() { return count; }
