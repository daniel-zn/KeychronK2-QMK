#!/bin/sh
# Builds the emulation image the Renode tests run: the ANSI default keymap with
# two changes that only matter under Renode. Never flash it.
#   * CORTEX_ALTERNATE_SWITCH: ChibiOS switches threads through PendSV.
#     Renode's Cortex-M0 never takes the NMI the default ARMv6-M port uses
#     after an interrupt, and the firmware stops at the first one.
#   * Short idle timeouts (backlight 10 s, sleep 20 s instead of 5 and 10
#     minutes), so the sleep tests take seconds.
# Run from the repository root inside the QMK toolchain.
set -eu

FLAGS="-DCORTEX_ALTERNATE_SWITCH=TRUE -DK2_BT_RGB_TIMEOUT=10000 -DK2_BT_SLEEP_TIMEOUT=20000"

make keychron/k2/rgb/v2/ansi:default EXTRAFLAGS="$FLAGS" TARGET=keychron_k2_rgb_v2_ansi_default_renode
