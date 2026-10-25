#!/bin/sh
# Downloads the stock K2 v2 RGB images from the SonixQMK database into
# ./stock/, for going back to stock firmware. The images are Keychron's; keep
# them out of git.
set -eu
cd "$(dirname "$0")"
mkdir -p stock
base=https://raw.githubusercontent.com/SonixQMK/Mechanical-Keyboard-Database/main/stockFWs/Keychron/240B
for f in Keychron_K2_RGB_Rev2_ANSI_v1.04_240B.bin Keychron_K2_RGB_Rev2_ISO_v1.05_240B.bin \
         Keychron_K2_RGB_Rev2_JIS_v1.18_240B.bin; do
    curl -fsSL -o "stock/$f" "$base/$f"
done
ls -l stock
