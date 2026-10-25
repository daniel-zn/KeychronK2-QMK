#!/bin/sh
# Builds every K2 v2 RGB release image into an output directory, checks that
# none of them carries the debug console or the Bluetooth debug log, that the
# program ends below the wear-leveling EEPROM, and that some RAM is left, then
# writes SHA256SUMS. Run from the repository root inside the QMK toolchain:
#   keyboards/keychron/k2/rgb/v2/release/build_all.sh dist
set -eu

OUT=${1:-dist}

TARGETS="ansi:default ansi:default_via iso:default iso:default_via"

# Present only with CONSOLE_ENABLE=yes or -DITON_BT_DEBUG.
DEBUG_SYMBOLS='^(console_task|ConsoleReport|iton_bt_log)$'

# The EEPROM (2 KiB backing store) sits below the last 64-byte flash sector,
# which the platform keeps for itself: 0x10000 - 64 - 2048.
EEPROM_BASE=$((0xF7C0))
# Free RAM between the end of .bss and the end of RAM (the ChibiOS heap).
MIN_FREE_RAM=128

mkdir -p "$OUT"
for t in $TARGETS; do
    variant=${t%%:*}
    keymap=${t#*:}
    name=keychron_k2_rgb_v2_${variant}_${keymap}
    make "keychron/k2/rgb/v2/$variant:$keymap"
    elf=".build/$name.elf"
    if arm-none-eabi-nm "$elf" | awk '{print $NF}' | grep -Eq "$DEBUG_SYMBOLS"; then
        echo "$name: debug console or Bluetooth debug log in a release image" >&2
        exit 1
    fi
    # .data is stored in flash right after .rodata, at __textdata_base__.
    textdata=$(arm-none-eabi-nm "$elf" | awk '$3 == "__textdata_base__" {print $1}')
    data=$(arm-none-eabi-size -A "$elf" | awk '$1 == ".data" {print $2}')
    end=$((0x$textdata + data))
    if [ "$end" -ge "$EEPROM_BASE" ]; then
        printf '%s: program ends at 0x%X, inside the EEPROM at 0x%X\n' "$name" "$end" "$EEPROM_BASE" >&2
        exit 1
    fi
    heap=$(arm-none-eabi-size -A "$elf" | awk '$1 == ".heap" {print $2}')
    if [ "$heap" -lt "$MIN_FREE_RAM" ]; then
        echo "$name: only $heap bytes of RAM left" >&2
        exit 1
    fi
    printf '%s: flash used to 0x%X of 0x%X, %s bytes RAM free\n' "$name" "$end" "$EEPROM_BASE" "$heap"
    cp "$name.bin" "$OUT/"
done

# sha256sum on Linux, shasum on macOS; both print "<hash>  <name>".
if command -v sha256sum >/dev/null; then sha256="sha256sum"; else sha256="shasum -a 256"; fi
(cd "$OUT" && $sha256 ./*.bin | sed 's| \./| |' > SHA256SUMS)
cat "$OUT/SHA256SUMS"
