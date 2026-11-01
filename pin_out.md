# Pinout Reference

This document summarizes the GPIO/electrical pin assignments used by the **Voltage Drop Detector** firmware (`src/main.cpp`).

> **Note on naming:** The 7 LEDs are labeled **P7 → P1** in the code comments, meaning `P7` is the lowest-voltage LED (GPIO 32) and `P1` is the highest-voltage LED (GPIO 12). The LED array is defined as `{32, 33, 25, 26, 27, 14, 12}` — index 0 maps to P7 and index 6 maps to P1.

---

## Complete Pin Assignment

| Function | GPIO | Signal / Interface | Notes |
|---|---|---|---|
| **ZMPT101B OUT** | **GPIO 34** | ADC input (`ADC1_CH6`) | AC voltage sensing input only; nomux pin |
| **DHT11 DATA** | **GPIO 23** | GPIO / GPIO | DHT11 digital data line |
| **OLED SDA** | **GPIO 21** | I2C data | Wire library |
| **OLED SCL** | **GPIO 22** | I2C clock | Wire library |
| **OLED RESET** | **GPIO -1** | Internal | `OLED_RESET` macro; reset not externally connected |
| **Relay 1 (Protection)** | **GPIO 18** | GPIO output | Active-LOW module |
| **Relay 2 (Fan/Temp)** | **GPIO 19** | GPIO output | Active-LOW module |
| **LED P7** | **GPIO 32** | GPIO output | Lowest-voltage LED (index 0) |
| **LED P6** | **GPIO 33** | GPIO output | (index 1) |
| **LED P5** | **GPIO 25** | GPIO output | (index 2) |
| **LED P4** | **GPIO 26** | GPIO output | Nominal-band LED (index 3) |
| **LED P3** | **GPIO 27** | GPIO output | (index 4) |
| **LED P2** | **GPIO 14** | GPIO output | (index 5) |
| **LED P1** | **GPIO 12** | GPIO output | Overvoltage LED (index 6) |

---

## Pin-to-Function Map (from Code)

```
#define DHTPIN           23
#define DHTTYPE          DHT11

#define relay1Pin        18   // Relay 1 (Voltage Protection)
#define relay2Pin        19   // Relay 2 (Fan / Temperature Control)

#define SCREEN_WIDTH     128
#define SCREEN_HEIGHT    64
#define OLED_RESET       -1
#define SCREEN_ADDRESS   0x3C

const int ledPins[7] = {32, 33, 25, 26, 27, 14, 12};  // P7 → P1

const int zmptPin = 34;   // ZMPT101B OUT
```

### Blynk Virtual Pins

| Virtual Pin | Handler | Direction |
|---|---|---|
| V1 | Manual switch Relay 1 | Write (0/1) |
| V2 | Manual switch Relay 2 | Write (0/1) |
| V3 | Relay 1 mode (Auto=0 / Manual=1) | Write (0/1) |
| V4 | Relay 2 mode (Auto=0 / Manual=1) | Write (0/1) |
| V5 | Push voltage | Read/Blynk |
| V6 | Push temperature | Read/Blynk |
| V7 | Push humidity | Read/Blynk |
| V8 | Push relay 1 status | Read/Blynk |
| V9 | Push relay 2 status | Read/Blynk |

---

## Sensor Signal Flow

```
AC Mains ─── ZMPT101B (PT / amp) ─── GPIO 34 (ADC1) ─── RMS voltage calc
                                                │
                                                ▼
              ┌──────────────┬──────────────┐
              ▼              ▼              ▼
          DHT11 (GPIO 23)  OLED I2C (0x3C, 21/22)   WebServer (port 80)
              │
              ▼
   Temperature + Humidity
```

---

## Relay Wiring Check

Both relays are **active-LOW** logic. In firmware:

```cpp
digitalWrite(relay1Pin, relay1State ? LOW : HIGH);
digitalWrite(relay2Pin, relay2State ? LOW : HIGH);
```

- `relay1State == true`  → GPIO 18 driven **LOW** (relay energized)
- `relay2State == true`  → GPIO 19 driven **LOW** (relay energized)

If your relays are **active-HIGH**, change the two lines above to `HIGH` when `state` is true.

---

## LED Bar Graph Interpretation

```
GPIO 32 (P7)  <  GPIO 33 (P6)  <  GPIO 25 (P5)  <  GPIO 26 (P4)  <  GPIO 27 (P3)  <  GPIO 14 (P2)  <  GPIO 12 (P1)
 LOW VOLT     180–200 V                               215–240 V (normal)     250 V (clamp)          > 250 V (over)
```

---

## Quick Lookup: "Which pin does X use?"

- **Voltage sensing:** GPIO 34
- **Temperature/humidity:** GPIO 23
- **OLED display:** I2C GPIO 21 (SDA), GPIO 22 (SCL)
- **Relay 1:** GPIO 18
- **Relay 2:** GPIO 19
- **Voltage level LED row:** GPIO 32, 33, 25, 26, 27, 14, 12

---

## Notes

- No pin uses interrupts in the current implementation.
- GPIO 34 and GPIO 35 are **ADC-only (nomonix)** on ESP32; GPIO 34 is used for the ZMPT101B.
- The relay pins (18, 19) and LED pins (12, 14, 25–27, 32, 33) are all general-purpose GPIOs and may be remapped if your hardware requires it.
