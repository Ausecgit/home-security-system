#ifndef NET_H
#define NET_H
#include <cstdint>
#include <stddef.h>

bool net_init();
void net_loop();
bool net_publish_motion(const char* cam_id, unsigned long ts_ms,
                        int cx, int cy, int area,
                        const char* cls, uint32_t frame_id);
bool net_publish_status(const char* cam_id, const char* stage,
                        uint32_t frame_id);
bool net_upload_still(uint8_t* jpg, size_t jpg_len, uint32_t frame_id);
bool net_connected();

#endif
