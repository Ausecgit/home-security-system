#ifndef CAMERA_H
#define CAMERA_H
#include "esp_camera.h"

bool camera_init();
camera_fb_t* camera_grab();

#endif
