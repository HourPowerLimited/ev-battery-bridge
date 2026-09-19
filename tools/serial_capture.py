"""
Capture serial output from one or more ports and print it.

Usage:
  python tools/serial_capture.py                  # COM3 for 10s
  python tools/serial_capture.py --port COM3 COM4 # both ports
  python tools/serial_capture.py --secs 30        # longer capture
"""
import argparse
import serial
import threading
import time

def read_port(port, baud, secs, lines, lock):
    try:
        with serial.Serial(port, baud, timeout=1) as s:
            deadline = time.time() + secs
            while time.time() < deadline:
                line = s.readline()
                if line:
                    with lock:
                        lines.append(f"[{port}] {line.decode(errors='replace').rstrip()}")
    except Exception as e:
        with lock:
            lines.append(f"[{port}] ERROR: {e}")

parser = argparse.ArgumentParser()
parser.add_argument("--port", nargs="+", default=["COM3"])
parser.add_argument("--baud", type=int, default=115200)
parser.add_argument("--secs", type=int, default=10)
args = parser.parse_args()

lines, lock = [], threading.Lock()
print(f"Capturing {args.port} for {args.secs}s...")
print("-" * 60)

threads = [threading.Thread(target=read_port, args=(p, args.baud, args.secs, lines, lock)) for p in args.port]
for t in threads: t.start()
for t in threads: t.join()

for line in lines:
    print(line)
print("-" * 60)
print("Done.")
