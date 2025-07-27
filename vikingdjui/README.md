Create a venv with:
`python -m venv venv`

Use the venv with:
`. venv/bin/activate`
or on Windows
`venv/Scripts/activate`

Install requirements with:
`pip install -r vikingdjui/requirements.txt`

To start run:
# Auto-detect serial port (default behavior) - localhost only
uvicorn vikingdjui:app --reload

# Make visible on all network interfaces
uvicorn vikingdjui:app --reload --host 0.0.0.0

# Specify custom port and make visible on all interfaces
uvicorn vikingdjui:app --reload --host 0.0.0.0 --port 8080

# Specify a serial port in .env property (SERIAL_PORT)
