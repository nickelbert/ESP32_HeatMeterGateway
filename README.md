# Landis+Gyr Ultraheat T550 MQTT Gateway (ESP32-C3)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Status: Work in Progress](https://img.shields.io/badge/Status-Work%20in%20Progress-orange.svg)]()

> [!WARNING]
> **Work in Progress (WIP):** This project is currently under active development. Features, configuration options, and interfaces may change at any time. Testing and feedback are welcome, but use at your own risk.

A high-performance smart meter gateway for the **Landis+Gyr Ultraheat T550 (LUZ2)** heat meter. Built natively with **ESP-IDF v6.0 and FreeRTOS** in **PlatformIO** for the **LilyGO T-01C3** (ESP32-C3 in ESP-01 form factor).

---

## Features

- **Native ESP-IDF & FreeRTOS Architecture:** Non-blocking FreeRTOS tasks for optical meter reading, web server, telnet console, and MQTT client.
- **Optical IR Interface (IEC 62056-21):**
  - Wakeup sequence: 40x `0x00` null bytes + `/?!\r\n` at 300 Baud 7E1.
  - Identification reading and dynamic baud rate switching to 19200 Baud 7E1.
  - OBIS code line parser with automatic dual-value splitting (e.g. forward and return temperatures `9.4.0` and `9.4.1`).
- **Home Assistant Auto-Discovery:**
  - Automatically provisions 22 sensor entities (energy, volume, power, flow rate, temperatures, previous year values, operating hours, error hours, meter timestamp, serial number) and an interactive *"Read Meter Now"* button entity.
- **Web-Based Configuration UI (Port 80):**
  - Live configuration of WiFi credentials, MQTT broker settings, read interval, and dummy mode.
  - Persistent storage in ESP32-C3 **NVS (Non-Volatile Storage)**.
- **Dual Over-The-Air (OTA) Updates:**
  - **Push-OTA:** Upload and flash `firmware.bin` directly via the browser or cURL.
  - **Pull-OTA:** Trigger remote background firmware updates via HTTP URL.
  - Uses dual-slot OTA partitions (`app0` / `app1`) with rollback safety.
- **Telnet Remote Logging & Debug Console (Port 23):**
  - Live log monitoring over the network.
  - Test/Dummy mode: Feed simulated OBIS strings directly into the parser without an optical head.
  - Commands: `send` (force MQTT state publish), `update` (OTA ready signal).
- **Network Resiliency:**
  - WiFi Station mode with automated reconnection logic.
  - Automatic fallback to SoftAP mode (`ESP-HeatMeter-Setup` on `192.168.4.1`) if WiFi is unconfigured or unavailable.

---

## Pinout & Hardware Connection

Designed as a drop-in replacement for ESP-01 IR read heads using the **LilyGO T-01C3**:

| ESP-01 Header Pin | Signal Name | ESP32-C3 Pin | Function |
|---|---|---|---|
| **Pin 1** | GND | GND | Ground |
| **Pin 2** | TXD | `GPIO21` | IR Transmitter Diode (300 Baud 7E1 Wakeup) |
| **Pin 3** | GPIO2 | `GPIO2` / IO | Unused / Reserved |
| **Pin 4** | CH_PD / EN | EN | Chip Enable / 3.3V Pullup |
| **Pin 5** | GPIO0 / BOOT | `GPIO9` | Boot strapping pin |
| **Pin 6** | RST | RST | Reset |
| **Pin 7** | RXD | `GPIO20` | IR Phototransistor Receiver (19200 Baud 7E1 Data) |
| **Pin 8** | VCC | 3.3V | Power Supply |

---

## Building & Flashing

### Method 1: WebSerial Browser Flasher (Recommended for First-Time Setup)

You can flash the firmware directly from any Chromium-based browser (**Google Chrome**, **Microsoft Edge**, or **Opera**) without installing Python, PlatformIO, or any toolchains on your computer.

#### 1. 1-Click Installation (ESP Web Tools)
1. Open the **[WebSerial Flasher](https://nickelbert.github.io/ESP32_HeatMeterGateway/)** (or run locally: `python -m http.server --directory docs 8000` and open `http://localhost:8000/`).
2. Connect your LilyGO T-01C3 or ESP32-S3 via USB-to-UART adapter.
   - **Bootloader Mode (LilyGO T-01C3 / ESP-01):** Ensure `GPIO9` (Pin 5 / BOOT) is connected to `GND` while powering on or resetting the adapter if your programmer does not toggle DTR/RTS automatically.
3. Click **"Connect & Flash Device"** and select the serial COM port from the browser prompt.
4. The flasher automatically detects your chip family (`ESP32-C3` or `ESP32-S3`).
5. Select **"Install Landis+Gyr Ultraheat T550 Gateway"** and check *"Erase device"* for a clean first-time install.
6. Once complete, you can open the serial console directly in the browser to view startup logs at 115200 Baud.

#### 2. Generic WebSerial Tools (Adafruit / Espressif ESP Launchpad / esp.huhn.me)
If you prefer third-party browser tools like [Adafruit WebSerial ESPTool](https://adafruit.github.io/Adafruit_WebSerial_ESPTool/), [esp.huhn.me](https://esp.huhn.me/), or [Espressif ESP Launchpad](https://espressif.github.io/esp-launchpad/):

- **Single Factory Image (Easiest):**
  Download `firmware-esp32-c3-factory.bin` (or `firmware-esp32-s3-factory.bin`) from the [latest release](https://github.com/nickelbert/ESP32_HeatMeterGateway/releases) and flash it to offset:
  - Offset: **`0x0`**

- **Individual Partitions (Advanced):**
  | Partition File | Flash Offset | Description |
  |---|---|---|
  | `bootloader.bin` | `0x0` | ESP-IDF 2nd-stage Bootloader |
  | `partitions.bin` | `0x8000` | Partition Table (NVS, OTA slots) |
  | `ota_data_initial.bin` | `0xe000` | Initial OTA boot slot marker |
  | `firmware.bin` | `0x10000` | Application firmware |

---

### Method 2: Local PlatformIO Build & Flash (Command Line)

#### Requirements
- [PlatformIO Core](https://platformio.org/) or VSCode with the PlatformIO extension.

#### Initial Flash via USB
1. Connect your ESP32-C3 / LilyGO T-01C3 via USB-to-UART adapter.
2. Build and upload the project:
   ```bash
   pio run -e esp32-c3 --target upload
   ```
   *(Note: The build process automatically generates `firmware-factory.bin` in `.pio/build/esp32-c3/` via post-build script).*
3. Open the serial monitor (115200 Baud):
   ```bash
   pio device monitor -b 115200
   ```
---
### Over-The-Air (OTA) Updates (via WiFi)
Once the gateway is running and connected to your WiFi network, subsequent updates do not require USB:
- **Browser Upload (Push):** Open `http://<ESP-IP>/`, select `.pio/build/esp32-c3/firmware.bin` under *Firmware Update (Push)*.
- **Terminal Upload (Push):**
  ```powershell
  curl -X POST --data-binary "@.pio\build\esp32-c3\firmware.bin" http://<ESP-IP>/update
  ```
- **Remote Server Update (Pull):** Enter the firmware URL in the web UI under *Remote Update (Pull)* and click *Check & Pull Update*.
---

## Configuration

Connect to the setup access point **`ESP-HeatMeter-Setup`** and open **`http://192.168.4.1/`** in your browser (or `http://<ESP-IP>/` if already connected to WiFi):

- **WiFi SSID & Password**
- **MQTT Broker (IP/Host & Port)**
- **MQTT Username & Password**
- **MQTT State Topic** (default: `ultraheat/state`)
- **Read Interval** (default: `3600` seconds / 1 hour)
- **Dummy Mode** (enable to test via Telnet without IR communication)

Click **Save Configuration & Restart** to save settings to NVS and reboot into Station mode.

---

## Home Assistant Integration

When connected to MQTT, the gateway automatically discovers and creates the device **"Landis+Gyr T550"** with the following entities:

| Sensor Name | OBIS Code | Unit | Device Class | State Class |
|---|---|---|---|---|
| Heat Energy | `6.8` | MWh | energy | total_increasing |
| Volume | `6.26` | m³ | volume | total_increasing |
| Heat Energy (Previous Year) | `6.8*01` | MWh | energy | total_increasing |
| Volume (Previous Year) | `6.26*01` | m³ | volume | total_increasing |
| Power | `6.6` | kW | power | measurement |
| Power (Previous Year) | `6.6*01` | kW | power | measurement |
| Flow Rate | `6.33` | m³/h | volume_flow_rate | measurement |
| Flow Rate (Previous Year) | `6.33*01` | m³/h | volume_flow_rate | measurement |
| Flow Temperature | `9.4.0` | °C | temperature | measurement |
| Return Temperature | `9.4.1` | °C | temperature | measurement |
| Flow Temp (Previous Year) | `9.4*01.0` | °C | temperature | measurement |
| Return Temp (Previous Year) | `9.4*01.1` | °C | temperature | measurement |
| Measurement Interval | `6.35` | min | duration | — |
| Operating Hours | `6.31` | h | duration | total_increasing |
| Error Hours | `6.32` | h | duration | total_increasing |
| Error Hours (Previous Year) | `6.32*01` | h | duration | total_increasing |
| Error Code | `F` | — | — | — |
| Billing Date | `6.36` | — | timestamp | — |
| Meter Timestamp | `9.36` | — | timestamp | — |
| Serial Number | `9.20` | — | — | — |
| Asset Number | `9.21` | — | — | — |
| Nominal Flow | `9.24` | m³/h | volume_flow_rate | — |
| **Read Meter Now** | Button | — | update | Trigger immediate readout |

---

## Telnet Debug Console

Connect via port 23:
```bash
telnet <ESP-IP> 23
```
- **Simulate OBIS input:** Send raw lines like `6.8(0012.345*MWh)` or `9.4(052.1&037.4*C)` to test parsing.
- **Trigger publish:** Send `send` to publish current JSON to MQTT.
- **OTA Ready:** Send `update` to confirm update readiness.

---

## License & Attribution

This project is licensed under the **[MIT License](LICENSE)** - see the [LICENSE](LICENSE) file for details.

### Third-Party Libraries & Dependencies
This project uses several open-source libraries and frameworks. All third-party copyright notices and licenses are documented in detail in **[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)**:

- **[cJSON](https://github.com/DaveGamble/cJSON)** – [MIT License](THIRD_PARTY_NOTICES.md#1-cjson) (Copyright © Dave Gamble & cJSON contributors)
- **[esp-mqtt](https://github.com/espressif/esp-mqtt)** – [Apache License 2.0](THIRD_PARTY_NOTICES.md#2-esp-mqtt) (Copyright © Espressif Systems & Tuan PM)
- **[ESP-IDF Framework](https://github.com/espressif/esp-idf)** – [Apache License 2.0](THIRD_PARTY_NOTICES.md#3-esp-idf-framework) (Copyright © Espressif Systems)
- **[FreeRTOS Kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel)** – [MIT License](THIRD_PARTY_NOTICES.md#4-freertos-kernel) (Copyright © Amazon.com, Inc. / Real Time Engineers Ltd.)
- **[lwIP TCP/IP Stack](https://savannah.nongnu.org/projects/lwip/)** – [BSD-3-Clause](THIRD_PARTY_NOTICES.md#5-lwip) (Copyright © Swedish Institute of Computer Science)
- **[Unity Test Framework](https://github.com/ThrowTheSwitch/Unity)** – [MIT License](THIRD_PARTY_NOTICES.md#6-unity-test-framework) (Copyright © Mike Karlesky, Mark VanderVoord, Greg Williams)
- **[ESP Web Tools](https://github.com/esphome/esp-web-tools)** – [Apache License 2.0](THIRD_PARTY_NOTICES.md#7-esp-web-tools) (Copyright © Nabu Casa, Inc. / ESPHome contributors)

---

## AI Disclosure / Transparency Notice

Documentation and code components in this repository have been created with the assistance of generative AI coding tools and were reviewed, verified, and tested by the maintainer.
