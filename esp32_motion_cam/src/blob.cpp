#include "blob.h"
#include "config.h"
#include "esp_heap_caps.h"
#include <cstring>

static int W = 0, H = 0, P = 0;
static uint8_t* visited = nullptr;
static int* stack = nullptr;

bool blob_init(int w, int h) {
  W = w; H = h; P = w * h;
  visited = (uint8_t*)heap_caps_malloc(P, MALLOC_CAP_SPIRAM);
  stack = (int*)heap_caps_malloc((size_t)P * sizeof(int), MALLOC_CAP_SPIRAM);
  if (!visited || !stack) { blob_deinit(); return false; }
  return true;
}

void blob_deinit() {
  free(visited); free(stack);
  visited = nullptr; stack = nullptr;
  W = H = P = 0;
}

static const char* classify(int area) {
  if (area < SMALL_BLOB_PX) return "small";
  if (area < LARGE_BLOB_PX) return "medium";
  return "large";
}

BlobInfo blob_analyze(const uint8_t* mask, int w, int h, int min_pixels) {
  BlobInfo result = { false, 0, 0, 0, "none" };
  if (!mask || !visited || w != W || h != H) return result;
  memset(visited, 0, P);

  int best_area = 0, best_cx = 0, best_cy = 0;

  for (int seed = 0; seed < P; seed++) {
    if (mask[seed] == 0 || visited[seed]) continue;
    int sp = 0;
    stack[sp++] = seed;
    visited[seed] = 1;
    int area = 0;
    long sx = 0, sy = 0;
    while (sp > 0) {
      int p = stack[--sp];
      int px = p % W;
      int py = p / W;
      area++;
      sx += px;
      sy += py;
      int xn = px - 1, xp = px + 1;
      int yn = py - 1, yp = py + 1;
      if (xn >= 0) {
        int q = py * W + xn;
        if (mask[q] && !visited[q]) { visited[q] = 1; stack[sp++] = q; }
      }
      if (xp < W) {
        int q = py * W + xp;
        if (mask[q] && !visited[q]) { visited[q] = 1; stack[sp++] = q; }
      }
      if (yn >= 0) {
        int q = yn * W + px;
        if (mask[q] && !visited[q]) { visited[q] = 1; stack[sp++] = q; }
      }
      if (yp < H) {
        int q = yp * W + px;
        if (mask[q] && !visited[q]) { visited[q] = 1; stack[sp++] = q; }
      }
    }
    if (area > best_area) {
      best_area = area;
      best_cx = (int)(sx / area);
      best_cy = (int)(sy / area);
    }
  }

  if (best_area >= min_pixels) {
    result.found = true;
    result.cx = best_cx;
    result.cy = best_cy;
    result.area = best_area;
    result.classification = classify(best_area);
  }
  return result;
}
