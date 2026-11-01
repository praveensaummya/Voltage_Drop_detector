# Voltage Drop Detector

A firmware project for the **ESP32 Dev Module** that monitors AC supply voltage and ambient temperature using a **ZMPT101B** voltage transformer, a **DHT11** temperature/humidity sensor, and reports the readings through a local web interface, an **OLED display**, and the **Blynk** IoT cloud dashboard.

The core purpose of the project is to detect under-voltage and over-voltage conditions on an AC circuit and drive protective/indicator outputs accordingly (relays, LEDs, and an OLED status line).

---

## Table of Contents

- [Hardware Overview](#hardware-overview)
- [Pin Assignment](#pin-assignment)
- [Sensor & Output Summary](#sensor--output-summary)
- [Functionality](#functionality)
- [How It Works](#how-it-works)
- [Building & Uploading](#building--uploading)
- [Web Interface](#web-interface)
- [Blynk Dashboard](#blynk-dashboard)
- [Configuration](#configuration)
- [Project Structure](#project-structure)
- [Notes & Limitations](#notes--limitations)
- [License](#license)

---

## Hardware Overview

| Component | Purpose |
|---|---|---|---|
| **ESP32 Dev Module** | Main MCU running Arduino-ESP32 framework |
| **ZMPT101B** (Varley/Amplified AC Voltage Sensor) | Step-down AC voltage sensor; output feeds the ESP32 ADC |
| **DHT11** | Measures temperature and relative humidity |
| **SSD1306 OLED** (0.96" 128x64) | Local status display |
| **2x Relays** | Logical outputs: voltage protection (Relay 1) and fan/temperature control (Relay 2) |
| **7x LEDs** | Bar-graph voltage level indicator |
| **WiFi/Blynk** | Cloud telemetry and remote control |

---

## Pin Assignment

The following table lists every GPIO used in `src/main.cpp`.

| Function | Pin | Notes |
|---|---|---|
| **ZMPT101B OUT** (AC voltage) | **GPIO 34** | ADC-only input (ADC1_CH6), nomux |
| **DHT11 DATA** | **GPIO 23** | DHT11 sensor |
| **Relay 2 (Fan/Temp control)** | **GPIO 19** | Active-LOW module output |
| **Relay 1 (Voltage protection)** | **GPIO 18** | Active-LOW module output |
| **OLED SDA** | **GPIO 21** | I2C data |
| **OLED SCL** | **GPIO 22** | I2C clock |
| **OLED RESET** | **GPIO -1** (internal) | `OLED_RESET` macro |
| **OLED I2C ADDRESS** | `0x3C` | SSD1306 default |
| **LED P7 to P1** | **GPIO 32, 33, 25, 26, 27, 14, 12** | Bar graph, index 0 = P7 (lowest) to index 6 = P1 (highest) |

### LED Bar Graph Mapping (Top to Bottom)

```
GPIO 32  to  LED P7   (voltage < 180 V)
GPIO 33  to  LED P6   (180-200 V)
GPIO 25  to  LED P5   (200-215 V)
GPIO 26  to  LED P4   (215-240 V, nominal)
GPIO 27  to  LED P3   (240-250 V)
GPIO 14  to  LED P2   (250 V, clamp)
GPIO 12  to  LED P1   (> 250 V, overvoltage)
```

> **Note:** The array in code is {32, 33, 25, 26, 27, 14, 12}, so index 0 is the first pin (GPIO 32 = P7) and index 6 is the last (GPIO 12 = P1).

---

## Sensor & Output Summary

| Block | Library / Module | Trigger / Threshold |
|---|---|---|
| **Voltage (ZMPT101B)** | analogRead on GPIO 34 + 200 ms peak-to-peak window | < 180 V to LOW VOLT, > 250 V to OVERVOLT |
| **Temperature/Humidity (DHT11)** | DHT library on GPIO 23 | Fan ON if temp >= 30 C |
| **Relay 1 (Protection)** | GPIO 18 | Auto: ON when 180 V <= V <= 250 V; Manual: Blynk V1/V3 |
| **Relay 2 (Fan)** | GPIO 19 | Auto: ON when temp >= 30 C; Manual: Blynk V2/V4 |
| **LED Bar Graph** | GPIO 32-12 | 7-level voltage position indicator |
| **OLED (SSD1306)** | I2C (0x3C) | Live voltage, temp, humidity, and status string |
| **Web Server** | ESP32 WebServer on port 80 | / HTML, /readings JSON |
| **Blynk Cloud** | BlynkESP32 | 5 s data push; V1-V9 control & telemetry |

---

## Functionality

1. **Voltage measurement** - The ZMPT101B output is sampled over a 200 ms window. The peak-to-peak ADC value is converted to an RMS voltage estimate (rawPeakToPeak * 0.14) and displayed/monitored.
2. **Environment monitoring** - The DHT11 reports temperature and humidity once per loop to both the OLED and the web page.
3. **Auto relay control** - Relay 1 trips to protect the load when voltage falls outside the 180-250 V window; Relay 2 drives a fan whenever the temperature exceeds 30 C.
4. **Manual override** - Blynk virtual pins V1-V4 allow a user to force each relay's state or switch it between Auto/Manual mode on the fly.
5. **Local web UI** - An offline HTML page shows live voltage, temperature, and humidity, and auto-refreshes every 500 ms.
6. **OLED display** - Shows a compact status readout: voltage, temperature, humidity, and a status string.
7. **Blynk cloud dashboard** - Every 5 seconds, voltage, temperature, humidity, and relay states are pushed to Blynk, and can be controlled from the cloud app.

---

## How It Works

### Voltage Chain

```
AC Input - ZMPT101B (step-down transformer + amp) - GPIO 34 (ADC)
```

The ESP32 samples the ADC for 200 ms, tracks signalMax and signalMin, computes rawPeakToPeak = signalMax - signalMin, then estimates RMS voltage with a scaling factor of 0.14.

### Relay Logic (Auto Mode)

```
Relay 1 (protection):  ON  if 180 <= V <= 250
                       OFF otherwise

Relay 2 (fan):         ON  if temp >= 30 C
                       OFF otherwise
```

In **Manual mode**, Blynk writes override the above by setting manualR1 / manualR2 flags, and the relay mode variables (relay1Mode, relay2Mode) dictate whether the logic is auto or manual.

### Active-LOW Relay Output

The relays are **active-LOW** modules, so:

```cpp
digitalWrite(relay1Pin, relay1State ? LOW : HIGH);
digitalWrite(relay2Pin, relay2State ? LOW : HIGH);
```

When relay1State/relay2State is true, the output is driven LOW.

---

## Building & Uploading

This project uses **PlatformIO** with the Arduino framework for ESP32.

```bash
# Build
pio run

# Upload (port defined in platformio.ini)
pio run -t upload

# Monitor serial output (115200 baud)
pio device monitor
```

Required libraries (declared in platformio.ini lib_deps):
- me-no-dev/ESPAsyncWebServer
- adafruit/Adafruit SSD1306
- adafruit/DHT sensor library
- adafruit/Adafruit GFX Library
- khoih-prog/BlynkESP32_BT_WF

---

## Web Interface

The embedded HTML page (index_html in src/main.cpp) is served at /. It displays:

- **RMS Voltage** (V)
- **Temperature** (C)
- **Humidity** (%)

The page fetches /readings via fetch() every 500 ms and updates three reading boxes in real time. No external CSS/JS is loaded - the page is self-contained and served from PROGMEM.

---

## Blynk Dashboard

Blynk virtual pins used (with handlers in src/main.cpp):

| Virtual Pin | Purpose |
|---|---|
| V1 | Relay 1 manual switch (0 = OFF, 1 = ON) |
| V2 | Relay 2 manual switch (0 = OFF, 1 = ON) |
| V3 | Relay 1 mode (0 = Auto, 1 = Manual) |
| V4 | Relay 2 mode (0 = Auto, 1 = Manual) |
| V5 | Blynk push: RMS voltage |
| V6 | Blynk push: temperature |
| V7 | Blynk push: humidity |
| V8 | Blynk push: relay 1 state/status |
| V9 | Blynk push: relay 2 state/status |

sendBlynkData() runs on a 5-second timer, pushing sensor data and relay state to the Blynk dashboard.

---

## Configuration

Edit the following at the top of src/main.cpp:

```cpp
char auth[]  = "YOUR_BLYNK_TOKEN";   // Blynk cloud authentication token
char ssid[]  = "YOUR_WIFI_SSID";     // Local WiFi network
char pass[]  = "YOUR_WIFI_PASSWORD"; // WiFi password
```

The Blynk template is defined as:
```cpp
#define BLYNK_TEMPLATE_ID "TMPL6A8TFGztT"
#define BLYNK_TEMPLATE_NAME "Voltage and Temp Monitor"
```

---

## Project Structure

```
.
├── platformio.ini        # PlatformIO & ESP32 board configuration
├── src/
│   └── main.cpp           # Firmware entrypoint (sensors, relays, OLED, web, Blynk)
├── include/
│   └── README             # Included for PlatformIO's default header-file template
├── lib/
│   └── README             # Included for PlatformIO's default private-library template
├── test/
│   └── README             # PlatformIO Test Runner template
├── .vscode/
│   ├── launch.json        # PIO debug configurations
│   ├── extensions.json
│   └── c_cpp_properties.json
├── .gitignore
└── README.md             # This file
```

---

## Notes & Limitations

- **ADC accuracy:** The voltage scaling factor (0.14) is a rough calibration constant for the specific ZMPT101B wiring/divider; verify and adjust it for your actual hardware.
- **GPIO 34** is an ADC-only input (nomux), so it cannot be used as a general GPIO.
- **DHT11** is a slow, low-precision sensor; expect ±2 C and ±5% humidity accuracy.
- **Relay type:** Both relays are active-LOW; verify your hardware modules are wired the same way.
- **WiFi/Blynk:** The firmware assumes a stationary access point; Blynk tokens are hard-coded for simplicity.

---

## License

This project is licensed under the [MIT License](LICENSE).

Copyright (c) 2026 Praveen Saummya

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

