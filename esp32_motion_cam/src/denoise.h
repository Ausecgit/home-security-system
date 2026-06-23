#ifndef DENOISE_H
#define DENOISE_H
#include <cstdint>

void blur_3x3(const uint8_t* src, uint8_t* dst, int w, int h);
void erode_mask(const uint8_t* src, uint8_t* dst, int w, int h);

#endif
