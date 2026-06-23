# Home Security System — Running Summary

## Architecture
- **Status board**: Raspberry Pi 3B with touchscreen (central hub)
- **Detection/AI**: MiniPC with Google Coral, running Frigate
- **Sensor cameras**: ESP32-S3-N16R8 + OV3660, running custom motion firmware
- **Networking**: ESP32-S2 mini acts as mock WiFi AP "DonsTest" during prototyping

## Network: DonsTest
- **SSID**: DonsTest
- **Password**: dontest123
- **Security**: WPA2
- **Channel**: 1
- **Served by**: ESP32-S2-Saola-1 (MAC cc:8d:a2:88:c1:62)
- **AP IP**: 192.168.4.1
- **AP status page**: http://192.168.4.1/ (shows connected stations + IPs)
- **Subnet**: 192.168.4.x (DHCP by ESP32-S2)

## Connected Devices
- **Pi 3B**: 192.168.4.2 (hostname: statusboard, MAC B8:27:EB:1E:04:97)
  - mosquitto MQTT broker on port 1883 (running)
  - statusboard Flask app on port 8080 (dashboard + still receiver, running)
  - SSH: pi@192.168.4.2 (password: donstest)
  - Fixed: /opt/statusboard/ ownership changed to pi:pi
- **S3 Camera**: 192.168.4.4 (MAC 82:E9:3B:80:80:C0)
  - Running motion camera firmware
  - Building background model (needs 30s for first frame, 10min for full N=20)
  - Serial USB CDC doesn't work on GOOUU board (USB-Serial/JTAG mode)
  - No LED feedback (WS2812 on GPIO48, not simple LED)

## ESP32-S2 AP firmware (`/home/t15/Desktop/esp32s2_ap`)
- **Board**: ESP32-S2-Saola-1, 4MB flash, 2MB PSRAM
- **Framework**: Arduino + PlatformIO
- **Status**: BUILT + FLASHED. AP broadcasting verified.
- **Serial**: USB CDC (HWCDC) at 115200 baud, prints DHCP leases every 10s
- **Note**: To re-flash, hold BOOT + press RESET to enter bootloader mode
- **Web page**: http://192.168.4.1/ shows connected stations with MAC + IP

## ESP32 motion camera firmware (`/home/t15/Desktop/esp32_motion_cam`)
- **Board**: GOOUU ESP-32-S3-Cam V1.5 (ESP32-S3-WROOM-1 N16R8)
  - 16MB flash, 8MB OPI PSRAM
  - Camera pinout = CAMERA_MODEL_ESP32S3_EYE
  - WS2812 RGB LED on GPIO48 (not simple LED, digitalWrite won't work)
  - USB-Serial/JTAG interface (303a:1001) — Serial CDC doesn't output
  - Boot button on GPIO0, shutter on GPIO46, user button on GPIO3
- **Sensor**: OV3660, native YUV422 output
- **Detection resolution**: QVGA 320x240
- **Still capture resolution**: VGA 640x480 (JPEG, POSTed over HTTP)
- **Background model**: Sliding window of N=20 frames, one every 30s
  - Per-pixel Y/U/V running mean + variance (sums/squared-sums in PSRAM)
  - z-score detection: pixel flagged if any channel z > 3.0
  - Background FREEZE during motion (10s cooldown)
- **Blob analysis**: Flood fill, reports centroid + area + classification
- **Reporting**: MQTT to Pi (home/cam/<id>/motion) + HTTP POST still to Pi:8080/still
- **Framework**: Arduino + PlatformIO
- **Status**: FULLY OPERATIONAL + DENOISED. Motion events + still images flowing to Pi.
- **Z_THRESHOLD**: 5.0
- **Denoise pipeline** (all enabled):
  - 3x3 spatial blur on each YUV plane before z-score (reduces sensor noise)
  - Morphological erosion on motion mask (removes isolated pixels)
  - Temporal persistence: requires motion in 2 consecutive frames (AND of current + previous mask)
  - AE/AWB/Gain lock after 5 background frames (prevents brightness shifts)
  - MIN_BLOB_PIXELS raised from 64 to 200
- **Results**: Zero false positives when idle, immediate detection on real motion
- **Still upload**: fmt2jpg returns error code 1 but produces valid JPEG — code checks output not error code
- **Stills received**: 200+ JPEGs on Pi at /opt/statusboard/stills/
- **Serial**: USB CDC doesn't work on GOOUU board. ARDUINO_USB_CDC_ON_BOOT=1 crashes the board — DO NOT USE.
- **Debugging**: Use MQTT heartbeats (published every 10s with class="running", includes ae status)
- **Note**: Always physically power-cycle S3 after flashing (esptool reset unreliable on USB-Serial/JTAG)
- **TODO before flash**: Edit config.h:
  - WIFI_SSID → DonsTest
  - WIFI_PASS → dontest123
  - MQTT_HOST → 192.168.4.2
  - STILL_ENDPOINT → http://192.168.4.2:8080/still
  - Camera pins for your ESP32-S3 board

## Raspberry Pi Status Board
- **Hardware**: Raspberry Pi 3B + touchscreen
- **OS**: Raspberry Pi OS Lite 64-bit (Debian 13/trixie, 2026-06-18)
- **IP**: 192.168.4.2 on DonsTest
- **Hostname**: statusboard
- **SSH**: pi / donstest (sudo NOPASSWD)
- **Services**:
  - **mosquitto** (port 1883, anonymous, listener 0.0.0.0)
  - **statusboard** Flask app (port 8080):
    - Dashboard: http://192.168.4.2:8080/ (auto-refresh, shows events + stills)
    - Still receiver: POST http://192.168.4.2:8080/still
    - Serves stills: http://192.168.4.2:8080/stills/<filename>
  - **nginx** (installed, available for reverse proxy)
  - **avahi-daemon** (mDNS, statusboard.local)
- **Custom systemd services**:
  - pi-startup.service: runs /opt/startup.sh on boot (rfkill, WiFi, mosquitto, statusboard)
  - mosquitto.service.d/override.conf: Type=simple (was Type=notify)
- **Status**: FULLY OPERATIONAL on DonsTest

## Tasks Status
- [x] Install PlatformIO toolchain on this laptop
- [x] Flash ESP32-S2 mini as WiFi AP "DonsTest" (WPA2/dontest123)
- [x] Write ESP32-S3 motion camera firmware
- [x] Flash Raspberry Pi OS Lite to microSD
- [x] Install all packages on Pi (mosquitto, Flask, paho-mqtt, nginx, etc.)
- [x] Pi connected to DonsTest at 192.168.4.2
- [x] mosquitto running on Pi
- [x] statusboard Flask app running on Pi
- [ ] Verify Pi accessible from DonsTest (SSH + web)
- [x] Build + flash motion camera firmware to ESP32-S3
- [x] Pi statusboard running (fixed /opt/statusboard permissions)
- [x] S3 camera connected to DonsTest at 192.168.4.4
- [x] End-to-end test: motion detected → MQTT events + still images → Pi dashboard
- [x] Fix still image HTTP upload (fmt2jpg returns err=1 but produces valid JPEG)
- [x] Tune Z_THRESHOLD (3.0 → 5.0, eliminated full-frame false positives)
- [ ] Next: MiniPC + Coral + Frigate integration
- [ ] Next: Additional ESP32-S3 cameras
- [ ] Next: Pi touchscreen display setup

## Key Files on Pi
- /opt/statusboard/app.py — Flask status board app
- /opt/startup.sh — Boot script (rfkill, WiFi, services)
- /etc/mosquitto/conf.d/donstest.conf — Mosquitto config
- /etc/systemd/system/pi-startup.service — Startup service
- /etc/systemd/system/mosquitto.service.d/override.conf — Type=simple override
- /etc/systemd/system/statusboard.service — Statusboard systemd service
- /opt/pi_debs/ — 46 .deb files used for offline install

## Credentials Summary
- DonsTest WiFi: dontest123
- Pi SSH: pi / donstest
- Pi hostname: statusboard.local (via avahi) or 192.168.4.2
- MQTT broker: 192.168.4.2:1883 (anonymous)
- Status board: http://192.168.4.2:8080/
- Still receiver: POST to http://192.168.4.2:8080/still

## How to re-flash ESP32-S2
1. Hold BOOT, press & release RESET, release BOOT
2. esptool --port /dev/ttyACM0 --baud 460800 write-flash 0x1000 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin

## Hardware detected on this laptop
- ESP32-S2-Saola-1 on /dev/ttyACM0 (USB 303a:0002, 4MB flash, 2MB PSRAM)
- microSD 29.7G at /dev/mmcblk0 (Pi OS Lite installed)
- Laptop: Fedora, Python 3.14.5, PlatformIO 6.1.19, esptool 5.3.0

## Files created
- `/home/t15/Desktop/PROJECT_SUMMARY.md` — this file
- `/home/t15/Desktop/esp32s2_ap/` — ESP32-S2 AP firmware project
- `/home/t15/Desktop/esp32_motion_cam/` — ESP32-S3 motion camera firmware project
