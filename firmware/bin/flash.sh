#!/bin/bash
echo "========================================================"
echo "    RPMLOOP Navigator - ESP32-S3 Flash Utility (Linux/macOS)"
echo "========================================================"
read -p "Enter ESP32-S3 Serial Device (e.g. /dev/ttyUSB0 or /dev/ttyACM0): " PORT
if [ -z "$PORT" ]; then
    echo "[ERROR] Port cannot be empty!"
    exit 1
fi

esptool.py --chip esp32s3 -p "$PORT" -b 460800 --before default-reset --after hard-reset write-flash --flash-mode dio --flash-size 4MB --flash-freq 80m 0x0 bootloader.bin 0x8000 partition-table.bin 0x10000 rpmloop_navigator.bin
