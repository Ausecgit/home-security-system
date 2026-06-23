#ifndef BACKGROUND_H
#define BACKGROUND_H
#include <cstdint>
#include <stddef.h>

bool bg_init(int width, int height, int window_n);
void bg_deinit();
void bg_push_frame(const uint8_t* y, const uint8_t* u, const uint8_t* v);
void bg_compute_mask(const uint8_t* y, const uint8_t* u, const uint8_t* v, uint8_t* mask_out);
bool bg_ready();
int bg_count();

#endif
