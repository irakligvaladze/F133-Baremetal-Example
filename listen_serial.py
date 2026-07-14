import serial
import sys

# usage:
# python capture_raw.py COM7 dump.bin
# or on Linux:
# python3 capture_raw.py /dev/ttyUSB0 dump.bin

port = sys.argv[1]
out_file = sys.argv[2]

ser = serial.Serial(
    port,
    115200,
    timeout=10,
    xonxoff=False,
    rtscts=False,
    dsrdtr=False,
)

print("Waiting for RAW marker...")

while True:
    line = ser.readline()
    print(line.decode(errors="replace").rstrip())

    if line.startswith(b"RAW "):
        total = int(line.split()[1], 16)
        break

print(f"Reading {total} raw bytes...")

data = bytearray()

while len(data) < total:
    chunk = ser.read(total - len(data))
    if not chunk:
        raise TimeoutError(f"Timeout after {len(data)} / {total} bytes")
    data.extend(chunk)

with open(out_file, "wb") as f:
    f.write(data)

print(f"Wrote {len(data)} bytes to {out_file}")