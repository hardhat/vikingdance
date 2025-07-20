# Viking Dance Necklace Control System

This project enables a DJ to control a group of wearable ESP8266-based LED necklaces from a DJ booth using a web interface. The system consists of three main components:

## System Overview

- **Necklaces:** Each necklace is built around an ESP8266 (e.g., ESP-01S) and a chain of WS2812B LEDs. The necklaces receive wireless commands via ESP-NOW and can display various light patterns, respond to sleep/dark commands, and report RSSI (signal strength) back to the DJ booth.

- **DJ Booth Controller:** An ESP32 (LilyGo T-Display) acts as the central controller. It receives commands from the laptop over USB serial, broadcasts them to all necklaces via ESP-NOW, and displays status on its built-in screen. The ESP32 can also receive messages (such as RSSI reports) from the necklaces and relay them to the laptop.

- **Laptop Web UI:** A FastAPI-based web application provides a user-friendly control panel for the DJ. The web UI allows the DJ to select patterns, colors, BPM, and send special commands (dark, sleep, RSSI) to all necklaces. The laptop communicates with the ESP32 over USB serial.

## How It Works

1. **Wireless Control:**
   - The DJ uses the web UI to send commands (pattern, color, etc.) from the laptop.
   - The laptop sends these commands over USB serial to the ESP32 in the DJ booth.
   - The ESP32 broadcasts the commands to all necklaces using ESP-NOW.
   - Necklaces receive the commands and update their LED patterns accordingly.

2. **Feedback from Necklaces:**
   - Necklaces can send messages (such as RSSI signal strength) back to the ESP32.
   - The ESP32 prints these messages to the serial console, where the laptop web UI can parse and display them.

3. **Local Override:**
   - Each necklace has a button for local override (pattern cycling or sleep mode).

## Hardware

- 30x ESP8266-based necklaces (WS2812B LEDs, button on GPIO0)
- 1x ESP32 LilyGo T-Display (DJ booth controller)
- Laptop running Python 3, FastAPI, and a web browser

## Software Structure

- `vikingnecklaceesp8266/` - Firmware for the ESP8266 necklaces
- `vikingdjesp32/` - Firmware for the ESP32 DJ booth controller (T-Display)
- `vikingdjui/` - FastAPI web UI and backend for the laptop

## Quick Start

1. Flash the ESP8266 and ESP32 with their respective firmware.
2. Connect the ESP32 T-Display to the laptop via USB.
3. Set up the Python environment and install requirements (see `vikingdjui/README.md`).
4. Start the web UI and control the necklaces from your browser!

## License
BSD 3-Clause License. See LICENSE for details.
