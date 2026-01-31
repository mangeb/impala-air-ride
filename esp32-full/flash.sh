#!/bin/bash
# Flash firmware to ESP32-S3 and open serial monitor
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Find the USB modem port
PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)
if [ -z "$PORT" ]; then
    echo "ERROR: No USB modem device found. Is the ESP32 plugged in?"
    exit 1
fi
echo "Found device: $PORT"

# Kill any existing screen sessions on this port
PIDS=$(lsof -t "$PORT" 2>/dev/null || true)
if [ -n "$PIDS" ]; then
    echo "Killing processes holding $PORT: $PIDS"
    echo "$PIDS" | xargs kill -9 2>/dev/null || true
    sleep 1
fi

echo "==============================="
echo "  Flashing to $PORT"
echo "==============================="
cd "$SCRIPT_DIR"
python3 -m platformio run -t upload --upload-port "$PORT"

echo ""
echo "==============================="
echo "  Opening serial monitor"
echo "==============================="
sleep 1
screen "$PORT" 115200
