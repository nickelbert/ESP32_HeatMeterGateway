# Project: Landis+Gyr Ultraheat T550 MQTT Gateway for ESP32-C3 (ESP-IDF & FreeRTOS)

This project is a PlatformIO ESP-IDF C++ application for the ESP32-C3 (LilyGO T-01C3 / ESP-01 form factor). It reads data from a Landis+Gyr Ultraheat T550 heat meter via an infrared optical head, parses OBIS telegrams, and publishes the metrics over MQTT to Home Assistant with automatic MQTT discovery.

---

## 1. Hardware & Pinout

* **Target MCU:** ESP32-C3 (RISC-V architecture, 160 MHz, 4MB Flash)
* **Form Factor:** ESP-01 / LilyGO T-01C3
* **UART Configuration:** UART_NUM_1
  * **RX Pin:** GPIO 20 (Connected to IR Receiver)
  * **TX Pin:** GPIO 21 (Connected to IR Transmitter)
* **Optical Readout Protocol:**
  * Wakeup sequence: 40x `0x00` bytes followed by `/?!\r\n` at **300 Baud (7E1)**.
  * Data readout: Switches dynamically to **19200 Baud (7E1)** after receiving meter identifier.

---

## 2. Architecture & Modules

* **Framework:** ESP-IDF v5.x with FreeRTOS (C++).
* **`ConfigManager`:** Manages persistent settings stored in Non-Volatile Storage (NVS):
  * Wi-Fi SSID / Password
  * MQTT Broker Host, Port, Credentials, and Topic
  * Readout Interval (seconds)
  * Dummy Simulation Mode toggle
* **`WifiManager`:**
  * Connects to configured Wi-Fi in Station (STA) mode.
  * Handles disconnections and periodic background reconnects.
  * Fallbacks automatically to SoftAP mode (`ESP-HeatMeter-Setup`, IP: `192.168.4.1`) when disconnected or unconfigured.
* **`WebServer`:**
  * Built with ESP-IDF `esp_http_server` on Port 80.
  * HTML dashboard for configuration management and manual reboot trigger.
  * **Push-OTA:** HTTP POST binary upload directly from the web browser.
  * **Pull-OTA:** Background firmware download from a remote HTTP(S) URL.
* **`TelnetServer`:**
  * FreeRTOS BSD socket server listening on Port 23.
  * Live log streaming and debug terminal.
  * Test & Simulation interface for feeding OBIS test strings into the parser without physical meter hardware.
* **`MeterT550`:**
  * State-machine-driven UART reader (Idle $\rightarrow$ Wakeup $\rightarrow$ ReadIdent $\rightarrow$ ReceivingData).
  * Robust OBIS parser with dual-value splitting (`&`, e.g. forward/return temperature).
  * In `m_dummyMode`, physical UART transmission is bypassed to allow software simulation via Telnet.
* **`MqttHandler`:**
  * Publishes formatted JSON sensor telemetry to `ultraheat/state`.
  * Sends Home Assistant MQTT Auto-Discovery configuration for all supported OBIS entities.
  * Subscribes to `ultraheat/command` for manual triggers (e.g. `READ`).

---

## 3. Coding Standards & Conventions

All contributions and AI assistants must strictly follow the project coding standards, naming conventions, and formatting guidelines defined in [CODING_STANDARDS.md](../CODING_STANDARDS.md).

---

## 4. Licensing & Third-Party Dependencies

* **Project License:** This project is published under the **MIT License**.
* **License Compatibility Requirement:** Only third-party libraries and components with **MIT-compatible, permissive licenses** (e.g., MIT, Apache 2.0, BSD-2-Clause, BSD-3-Clause, ISC, Unlicense, Boost) are permitted.
* **Strict Copyleft Prohibition:** Copyleft-licensed components (such as **GPL**, **AGPL**, or restrictive **LGPL**) must **NEVER** be introduced or linked into this codebase.
* **Documentation Obligation:** Whenever a new external component or library is added, its license, copyright holder, repository URL, and full license text must be documented in [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).

