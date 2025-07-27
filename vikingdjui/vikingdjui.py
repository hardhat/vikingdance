import serial
import serial.tools.list_ports
import asyncio
import sys
from fastapi import FastAPI, Request, Form
from fastapi.responses import HTMLResponse, RedirectResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates
import threading
import re
from dotenv import load_dotenv
import os

# Load environment variables from .env file
load_dotenv()

app = FastAPI()

# Mount static files (CSS, JS if any)
app.mount("/static", StaticFiles(directory="static"), name="static")

# Jinja2 templates
templates = Jinja2Templates(directory="templates")

# Serial device (auto-detect for Windows/Linux)
def find_serial_port():
    ports = list(serial.tools.list_ports.comports())
    for port in ports:
        # Optionally filter for common USB-serial chips
        if ("USB" in port.description or "UART" in port.description or
            "CP210" in port.description or "CH340" in port.description or
            "Silicon Labs" in port.manufacturer if port.manufacturer else False):
            return port.device
    if ports:
        return ports[0].device  # fallback: first port
    raise RuntimeError("No serial ports found")

# Use SERIAL_PORT from .env if provided, otherwise auto-detect
SERIAL_PORT = os.getenv("SERIAL_PORT")
if SERIAL_PORT:
    print(f"Using SERIAL_PORT from .env: {SERIAL_PORT}")
else:
    SERIAL_PORT = find_serial_port()
    print(f"Auto-detected serial port: {SERIAL_PORT}")

BAUDRATE = 115200

ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1)

# --- Helper Functions ---
def send_serial_command(cmd: str):
    print(f"Sending: {cmd}")
    ser.write((cmd + '\n').encode())
    ser.flush()

def serial_reader():
    while True:
        try:
            line = ser.readline().decode(errors='replace').strip()
            if line:
                print(f"[SERIAL] {line}")
                if line.startswith("[NECKLACE]") and "RSSI" in line:
                    # Example: [NECKLACE] From: XX:XX:XX:XX:XX:XX | Bytes: N | Data: ... | RSSI: -XX
                    # Extract RSSI value if present
                    match = re.search(r"RSSI[:=]\s*(-?\d+)", line)
                    if match:
                        rssi = int(match.group(1))
                        print(f"[RSSI] Received RSSI: {rssi}")
        except Exception as e:
            print(f"[SERIAL ERROR] {e}")

# Start serial reader in background
threading.Thread(target=serial_reader, daemon=True).start()

# --- Routes ---
@app.get("/", response_class=HTMLResponse)
async def control_panel(request: Request):
    return templates.TemplateResponse("control.html", {"request": request})

@app.post("/send")
async def send(
    request: Request,
    pattern: int = Form(...),
    primary_r: int = Form(...), primary_g: int = Form(...), primary_b: int = Form(...),
    secondary_r: int = Form(...), secondary_g: int = Form(...), secondary_b: int = Form(...),
    bpm: int = Form(...), flags: int = Form(...)
):
    cmd = f"SET {pattern} {primary_r} {primary_g} {primary_b} {secondary_r} {secondary_g} {secondary_b} {bpm} {flags}"
    send_serial_command(cmd)
    return RedirectResponse("/", status_code=303)

@app.post("/dark")
async def go_dark():
    send_serial_command("DARK")
    return RedirectResponse("/", status_code=303)

@app.post("/rssi")
async def request_rssi():
    send_serial_command("RSSI")
    return RedirectResponse("/", status_code=303)

@app.post("/sleep")
async def send_sleep():
    send_serial_command("SLEEP")
    return RedirectResponse("/", status_code=303)
