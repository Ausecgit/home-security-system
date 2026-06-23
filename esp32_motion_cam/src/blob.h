#ifndef BLOB_H
#define BLOB_H
#include <cstdint>

struct BlobInfo {
  bool found;
  int cx;
  int cy;
  int area;
  const char* classification;
};

BlobInfo blob_analyze(const uint8_t* mask, int w, int h, int min_pixels);
bool blob_init(int w, int h);
void blob_deinit();

#endif
