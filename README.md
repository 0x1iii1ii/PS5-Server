# PS5-Server
 
This project is designed for the ESP series of devices to provide a WiFi HTTP(S) server and DNS server utilizing recent developments including UMTX2 and etaHEN.  

The following devices are currently supported:

* ESP8266
* ESP32 / ESP32 Dongle
* ESP32-S2 / ESP32-S2 Dongle
* ESP32-S3 / LILYGO T-Dongle-S3

Supports PS5 FW from 1.XX - 5.XX only.

## How to use

Note: ESP must have flash 4MB or more

Flash the .bin file to your ESP using nodemcu pyflasher (found in the Releases section), make sure to tick wipes all data and leave everything to defaults

Connect to ESP Wi-Fi with:

SSID: PS5_ESP_HOST
PASS: 12345678

You can optionally use the `esphost_media.pkg` (found in the Releases section) or you can go to `10.1.1.1` on your web or go to the User's Guide page to start the exploit!

A video walk-through for this project is located [here](https://www.youtube.com/watch?v=DybJIrBMcSo).

### Configuration for the D1 Mini PRO
- The CPU frequency must be set to **160 MHz**.  
![Board Information](https://github.com/stooged/PS5-Server/blob/main/images/board-info.jpg)
