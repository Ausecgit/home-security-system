#include "denoise.h"
#include <cstring>

void blur_3x3(const uint8_t* src, uint8_t* dst, int w, int h) {
  for (int y = 0; y < h; y++) {
    int y0 = y - 1, y1 = y, y2 = y + 1;
    bool top = y0 >= 0, bot = y2 < h;
    for (int x = 0; x < w; x++) {
      int x0 = x - 1, x1 = x, x2 = x + 1;
      bool left = x0 >= 0, right = x2 < w;
      int sum = 0, count = 0;
      if (top) {
        if (left)  { sum += src[y0 * w + x0]; count++; }
                    { sum += src[y0 * w + x1]; count++; }
        if (right) { sum += src[y0 * w + x2]; count++; }
      }
      if (left)  { sum += src[y1 * w + x0]; count++; }
                  { sum += src[y1 * w + x1]; count++; }
      if (right) { sum += src[y1 * w + x2]; count++; }
      if (bot) {
        if (left)  { sum += src[y2 * w + x0]; count++; }
                    { sum += src[y2 * w + x1]; count++; }
        if (right) { sum += src[y2 * w + x2]; count++; }
      }
      dst[y * w + x] = (uint8_t)(sum / count);
    }
  }
}

void erode_mask(const uint8_t* src, uint8_t* dst, int w, int h) {
  memcpy(dst, src, w * h);
  for (int y = 1; y < h - 1; y++) {
    for (int x = 1; x < w - 1; x++) {
      int i = y * w + x;
      if (!src[i]) { dst[i] = 0; continue; }
      if (!src[i - 1] || !src[i + 1] ||
          !src[i - w] || !src[i + w]) {
        dst[i] = 0;
      }
    }
  }
}
