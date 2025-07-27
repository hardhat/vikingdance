Create a venv with:
`python -m venv venv`

Use the venv with:
`. venv/bin/activate`
or on Windows
`venv/Scripts/activate`

Install requirements with:
`pip install -r vikingdjui/requurements.txt`

To start run:
# Auto-detect serial port (default behavior)
uvicorn vikingdjui:app --reload

# Specify a serial port in .env property (SERIAL_PORT)
