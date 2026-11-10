#!/bin/bash
# Regenera web/firmware/tamapoke.bin (firmware combinado) para el instalador web.
# Uso: bash tools/build_web.sh         # Waveshare ESP32-S3-Touch-AMOLED-1.75C (por defecto)
#      bash tools/build_web.sh 175     # Waveshare ESP32-S3-Touch-AMOLED-1.75 (con microSD)
# Ojo: el firmware de una placa NO arranca la pantalla en la otra (pines de reset distintos).
set -e
cd "$(dirname "$0")/.."
FQBN="esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB"
FLAGS=""
[ "$1" = "175" ] && FLAGS="-DBOARD_AMOLED_175"

echo "Compilando (${FLAGS:-1.75C})..."
arduino-cli compile --fqbn "$FQBN" --build-property "compiler.cpp.extra_flags=$FLAGS" --export-binaries .

B=build/esp32.esp32.esp32s3
echo "Fusionando binarios..."
esptool.py --chip esp32s3 merge-bin -o web/firmware/tamapoke.bin \
  0x0     "$B/TamaPoke.ino.bootloader.bin" \
  0x8000  "$B/TamaPoke.ino.partitions.bin" \
  0xe000  "$B/boot_app0.bin" \
  0x10000 "$B/TamaPoke.ino.bin"

echo "OK -> web/firmware/tamapoke.bin ($(du -h web/firmware/tamapoke.bin | cut -f1))"

echo "Empaquetando sprites..."
python3 tools/pack_bundle.py
