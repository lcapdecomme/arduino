#!/bin/bash
SKETCH_DIR="$(dirname "$0")"
DATA_DIR="$SKETCH_DIR/data"
MKLITTLEFS=~/.arduino15/packages/esp32/tools/mklittlefs/3.0.0-gnu12-dc7f933/mklittlefs
PORT="/dev/ttyUSB0"  # Changez si nécessaire
BAUD=921600

# Créer l'image LittleFS
$MKLITTLEFS -c "$DATA_DIR" -p 256 -b 4096 -s 0x160000 /tmp/littlefs.bin

# Uploader
python3 ~/.arduino15/packages/esp32/tools/esptool_py/4.5.1/esptool.py \
  --chip esp32 --port $PORT --baud $BAUD \
  write_flash 0x290000 /tmp/littlefs.bin

echo "Upload terminé!"
