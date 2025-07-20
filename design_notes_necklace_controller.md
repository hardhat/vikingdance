# ESP8266 Necklace Control System - Design Overview

This document outlines the architecture and design considerations for a wireless ESP8266-based wearable lighting system controlled via ESP-NOW. It also includes a Python FastAPI web interface to manage devices from a DJ booth connected to an ESP32 over USB.

---

## 🎯 Goals

- Control 30 wearable ESP8266 necklaces, each with 5 WS2812 LEDs.
- Use ESP-NOW to broadcast and send individual commands.
- Support both synchronized and independent light patterns.
- Enable location-aware functionality using RSSI.
- Offer a reliable local override and power-saving mode.

---

## 📦 Hardware Overview

- **Devices**: 30 ESP8266-based necklaces
  - **GPIO2** → WS2812 LED data line
  - **GPIO0** → Push button (used for mode toggle & sleep)
  - **RST** → Push button (wake from sleep)
- **Controller**: DJ booth with ESP32 Dev Board connected via USB to a laptop

---

## 🔌 Control Protocol

### ESP-NOW Command Structure (broadcast or per-device):

```c
struct Command {
  uint8_t target_mac[6];
  uint8_t mode;         // 0 = go dark, 1 = pattern, 2 = RSSI, 3 = sleep
  uint8_t pattern;      // chase, flash, twinkle, fade, etc.
  uint8_t primary[3];   // RGB
  uint8_t secondary[3]; // RGB
  uint8_t bpm;          // Beats per minute
  uint8_t flags;        // Bitmask for sync, random, etc.
  uint32_t timestamp;   // Sync alignment
};
```

---

## 🔧 Device Features

### ✅ Implemented

- WS2812 LED setup via FastLED
- ESP-NOW receiver
- Button press detection on GPIO0
- Long press (5s) → deep sleep until RST
- Local pattern cycling when in override mode
- Pattern display via command

### 🔜 Planned

- Beat-synchronized effects
- Real patterns: chase, twinkle, fade, etc.
- ACKs and retransmission logic
- OTA firmware updates (optional/future)
- Debug / diagnostic mode

---

## 📡 DJ Booth ESP32

### ✅ Implemented

- ESP-NOW broadcast commands via serial input
- USB serial communication to laptop
- Basic command parsing: SET, DARK, RSSI, SLEEP

### 🔜 Planned

- Device registration tracking (join events)
- Ping/heartbeat for presence monitoring
- Display device list and battery level

---

## 🖥️ Laptop Interface (FastAPI)

### ✅ Implemented

- HTML control panel via FastAPI + Jinja2
- Form to submit color, pattern, BPM, etc.
- Buttons for DARK, RSSI, SLEEP
- Serial communication to ESP32 backend

### 🔜 Planned

- Pattern name dropdown
- Live status dashboard (online devices, last seen, RSSI)
- Device-specific controls
- JSON API for mobile or advanced UI
- Auto-detection of serial port

---

## 📝 To-Do List

-

---

## 📁 Folder Structure (Suggested)

```
vikingdance/
├── LICENSE
├── README.md
├── design_notes_necklace_controller.md
├── vikingdjesp32/
│   └── vikingdjesp32.ino
├── vikingdjui/
│   ├── requirements.txt
│   ├── vikingdjui.py
│   ├── README.md
│   ├── templates/
│   │   └── control.html
│   └── static/
│       └── favicon.ico
├── vikingnecklaceesp8266/
│   └── vikingnecklaceesp8266.ino
```

---

## 📢 Notes

- ESP8266 devices must be pre-programmed with the DJ booth's MAC address (or broadcast)
- Devices wake on RST only and enter sleep on 5s GPIO0 hold
- Override mode ignores ESP-NOW and cycles patterns locally
- DJ booth (ESP32) handles broadcast + serial bridge

