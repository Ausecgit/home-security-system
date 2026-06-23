# Home Security System

ESP32-based home security system with custom motion detection, MQTT event publishing, and a Raspberry Pi status board dashboard.

## Architecture

| Component | Hardware | Role |
|-----------|----------|------|
| WiFi AP | ESP32-S2-Saola-1 | Mock network "DonsTest" (prototyping without home network) |
| Status Board | Raspberry Pi 3B + touchscreen | MQTT broker + Flask dashboard + still image receiver |
| Motion Camera | GOOUU ESP-32-S3-Cam V1.5 (OV3660) | Per-pixel z-score motion detection + still capture |
| AI Detection | MiniPC + Google Coral | Frigate (planned) |

## Motion Detection Algorithm

The camera firmware implements a statistical background subtraction pipeline:

1. **Frame capture**: OV3660 outputs YUV422 at QVGA (320x240)
2. **Denoise**: 3x3 spatial blur on each Y/U/V plane
3. **Background model**: Sliding window of N=20 frames (one sampled every 30s), per-pixel mean + variance via running sums
4. **Motion detection**: Per-pixel z-score per channel (z = |x - mean| / stdev), pixel flagged if any channel z > 5.0
5. **Mask cleanup**: Morphological erosion removes isolated pixels
6. **Temporal persistence**: Requires motion in 2 consecutive frames (AND of current + previous mask)
7. **AE/AWB lock**: Auto-exposure, white balance, and gain locked after 5 background frames to prevent brightness-induced false positives
8. **Blob analysis**: Flood fill finds largest blob, reports centroid + area + classification (small/medium/large)
9. **Cooldown**: 10-second freeze after detection (skips detection + background updates)

## Reporting

- **MQTT**: Motion events published to `home/cam/<id>/motion` as JSON `{cam, t, cx, cy, area, class, frame}`
- **Status**: Heartbeats published to `home/cam/<id>/status` every 10s
- **Stills**: JPEG captured on motion, POSTed to Pi's HTTP endpoint

## Projects

### `esp32_motion_cam/` — ESP32-S3 Motion Camera
- PlatformIO + Arduino framework
- ESP32-S3-WROOM-1 N16R8 (16MB flash, 8MB OPI PSRAM)
- OV3660 camera (CAMERA_MODEL_ESP32S3_EYE pinout)

### `esp32s2_ap/` — ESP32-S2 WiFi AP
- PlatformIO + Arduino framework
- Broadcasts "DonsTest" WPA2 network
- Web page at port 80 showing connected stations with IPs

## Network Configuration

- SSID: DonsTest / Password: dontest123
- AP (S2): 192.168.4.1
- Pi: 192.168.4.2 (MQTT:1883, Dashboard:8080)
- Camera: 192.168.4.x (DHCP)

## Key Files

- `esp32_motion_cam/src/config.h` — All tunables (WiFi, MQTT, thresholds, camera pins)
- `esp32_motion_cam/src/background.cpp` — Sliding-window background model
- `esp32_motion_cam/src/denoise.cpp` — Blur + morphological erosion
- `esp32_motion_cam/src/blob.cpp` — Flood-fill blob analysis
- `esp32_motion_cam/src/net.cpp` — WiFi + MQTT + HTTP still upload
- `PROJECT_SUMMARY.md` — Detailed running summary of the build process
