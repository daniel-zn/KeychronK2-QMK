# Keychron K2 v2 RGB: QMK with Bluetooth

QMK for the Keychron K2 version 2 with RGB backlight (SN32F248B MCU), wired
and Bluetooth, with every key remappable in VIA. The Bluetooth part is the
Keychron K8 port's (`keyboards/keychron/k8`), moved to the K2: the two boards
have the same MCU, the same ITON Bluetooth module on the same pins, the same
side switches and the same LED rows.

1. [Which K2 this is for](#1-which-k2-this-is-for)
2. [What changes compared to stock](#2-what-changes-compared-to-stock)
3. [Status](#3-status)
4. [Build](#4-build)
5. [Flash](#5-flash)
6. [Use it](#6-use-it)
7. [Back to stock](#7-back-to-stock)
8. [Tests](#8-tests)
9. [Files](#9-files)

## 1. Which K2 this is for

The **K2 version 2 with RGB backlight**, ANSI or ISO. Check before flashing:

| Check | K2 v2 RGB |
|---|---|
| Backlight | RGB (colours), not white-only |
| USB, on the cable, stock firmware (macOS: System Information → USB) | Manufacturer **Keychron**, product **Keychron K2**, ID 05ac:024f (ANSI) or 05ac:0250 (ISO) |
| Under the spacebar (keycap off) | Another K2 v2 RGB guide found the PCB marked "RGB-BC Ver 1.1" |

Not for: the K2 version 1 (says "Keytron" or "USB DEVICE"), the
white-backlight K2 (SN32F260), or the K2 Pro / Max / HE / version 3, which
already run Keychron's own QMK: remap those in the Keychron Launcher.

## 2. What changes compared to stock

| | Stock | This firmware |
|---|---|---|
| Remapping | None | Every key on 4 layers in VIA (Mac and Windows base + Fn layers), macros, live, no rebuild |
| Bluetooth | 3 profiles | 3 profiles: Fn+1/2/3, hold 3 s to pair |
| Mac / Windows switch | Yes | Yes, also told to the Bluetooth module |
| Battery | Charging light | Charging light (the charger drives it, unchanged), plus Fn+B shows the level on the number row and Esc blinks red when low |
| Sleep on Bluetooth | Yes | Backlight off after 5 min, deep sleep after 10 min, any key wakes, link stays up |
| Backlight | Keychron's effects | QMK's effects (about 40), set from VIA's Lighting tab |
| Caps Lock light | Yes | Yes, also over Bluetooth |
| Fn+Esc | — | Bootloader, for reflashing (the keyboard stops until replugged if pressed by accident; remap it in VIA if that is a worry) |

SonixQMK alone loses Bluetooth on the K2 (it has no Bluetooth driver set up
for the K2), its Mac Fn+Esc does nothing (`Q_RESET` is a ChibiOS constant, not
a keycode), and it has no VIA keymap for the K2 on its current branch. All
three are fixed here.

What remains different from stock, all small:

* Over Bluetooth the module takes 6 keys at once (as stock); over USB, any number
* Mouse keys, if mapped, work over USB only (the module's protocol has no mouse report)
* A Caps Lock change made on the computer while the keyboard is in deep sleep
  can leave the Caps Lock light wrong until the next change (seen in the
  emulator; the K8 port saw it once on hardware)
* Battery life in sleep has not been measured against stock
* macOS shows the "identify your keyboard" assistant once, because the USB ID
  changes from Apple's to Keychron's

## 3. Status

| Done | How |
|---|---|
| Bluetooth wiring checked | Stock K2 and K8 firmwares (SonixQMK database) disassembled: SPI0 interrupt handler instruction-for-instruction identical, same A0 request / A1 direction lines, same `0xA6` control commands, same side-switch decoding, same flash-mode command. The K8 port is tested on real hardware with a Mac |
| Builds | ANSI and ISO, `default` and `default_via`; no warnings; `qmk lint` passes |
| Fits | VIA images use flash up to 0xE584 of 0xF7C0 (settings area above), 208–224 bytes RAM free (the hardware-tested K8 VIA image has 224) |
| Emulator tests | 61 checks pass, 0 fail, 1 known issue (the Caps Lock one above), see [Tests](#8-tests) |
| Flash script checks | Refuses K8 images, K2 v1 images, junk, layout mismatches, a stock keyboard that is not a K2 v2 (20 checks in `flash/test_flash_k2.py`) |
| Reproducible | A clean rebuild gives byte-identical images |

**Not yet done: running it on a real K2.** It cannot brick the keyboard: the
SN32F248B's bootloader is in ROM, so shorting the two BOOT pads left of the
spacebar switch while plugging in always gets back to a flashable state, and
stock firmware can be put back.

## 4. Build

macOS: `brew install qmk/qmk/qmk libusb pkgconf` (QMK's formula brings the ARM
compiler). Debian/Ubuntu: `apt install gcc-arm-none-eabi libnewlib-arm-none-eabi`
and `pip install qmk`. Then, from the repository root:

    git submodule update --init --depth 1 lib/chibios lib/chibios-contrib lib/printf
    keyboards/keychron/k2/rgb/v2/release/build_all.sh dist

That builds the four images (`keychron_k2_rgb_v2_{ansi,iso}_{default,default_via}.bin`)
into `dist/`, checks each fits below the settings area with RAM to spare and
has no debug console, and writes `dist/SHA256SUMS`. One image:

    make keychron/k2/rgb/v2/ansi:default_via

## 5. Flash

Once, build the flasher (SonixFlasherC 3.0.0 with the K8 port's macOS patch):

    keyboards/keychron/k8/flash/build_flasher.sh

Then, side switch on **Cable**, cable plugged in (US layout, with VIA):

    keyboards/keychron/k2/rgb/v2/flash/flash_k2.py dist/keychron_k2_rgb_v2_ansi_default_via.bin

| The keyboard runs | What happens |
|---|---|
| Stock firmware (first time) | The script sends the ISP command stock firmware accepts, so there is no need to open the case |
| This firmware (later) | Press **Fn+Esc** when the script asks, to enter the bootloader |
| The ROM bootloader | Flashed directly |

The script refuses an image that is not for a K2 v2 RGB, a layout mismatch,
and a stock keyboard whose USB strings are not a K2 v2's ("Keychron" /
"Keychron K2"; a K2 version 1 or a K8 says something else). It asks before
writing and waits for the keyboard to come back.

If anything goes wrong mid-flash: unplug, short the two BOOT pads left of the
spacebar switch with tweezers while plugging back in, and run the command
again (it flashes the ROM bootloader directly).

## 6. Use it

| Control | Action |
|---|---|
| Side switch on Cable | USB |
| Side switch on BT | Bluetooth, to the last selected profile |
| Side switch Mac / Windows | Mac or Windows layers |
| Fn+1 / Fn+2 / Fn+3 | Switch to Bluetooth profile 1 / 2 / 3 |
| Hold Fn+1 / 2 / 3 for 3 s | Put that profile into pairing mode |
| Fn+B | Battery level on the number row for 3 s (green 10 keys, yellow 6, red 3) |
| Fn+Esc | Bootloader (Mac and Windows layers) |

Moving the side switch restarts the keyboard. To pair: switch to BT, hold
Fn+1 until the 1 key blinks blue, connect to "Keychron K2" from the host.
White blinking is reconnecting; green or red after an action is connected or
not. Pairings made under stock firmware are kept by the module and may still
work.

**VIA:** open [usevia.app](https://usevia.app) in Chrome. In Settings, turn on
"Show Design tab". In Design, load `via_json/k2_ansi.json` (or `k2_iso.json`),
then remap in Configure. The Bluetooth keys are under Custom: Batt, BT1, BT2,
BT3.

## 7. Back to stock

    keyboards/keychron/k2/rgb/v2/flash/fetch_stock.sh
    keyboards/keychron/k2/rgb/v2/flash/flash_k2.py keyboards/keychron/k2/rgb/v2/flash/stock/Keychron_K2_RGB_Rev2_ANSI_v1.04_240B.bin

Keychron's program from the SonixQMK database, not a copy of what was on the
keyboard; ISO: `..._ISO_v1.05_240B.bin`.

## 8. Tests

Renode 1.17 runs the firmware on an emulated SN32F248B, with the K8 port's
chip models and Bluetooth module model (`keyboards/keychron/k8/sim/renode`):

    keyboards/keychron/k2/rgb/v2/sim/renode/build.sh
    RENODE=/path/to/renode keyboards/keychron/k2/rgb/v2/sim/renode/run.sh .build/keychron_k2_rgb_v2_ansi_default_renode.elf

The K2 suite is the K8 port's tests plus two for the K2 keymaps; result on
2026-10-25: 61 checks pass, 0 fail, 1 known issue. It covers start-up
(profile, then the OS 300 ms later), connecting, typing (six keys, media
keys), the Caps Lock light from the host, battery query and low battery,
pairing by holding Fn+1, profile switching and saving, cable and back, a
module that stops answering, deep sleep and wake with the link kept, no sleep
while pairing or with a USB host, typing while USB is suspended, the Mac Fn
layer's Bluetooth keys, and the Bluetooth keys doing nothing on the cable.
The K8 port's own suite also passes on the same setup (56 checks, 0 fail, the
same known issue). Not covered: the real radio and module timing, how the
LEDs look, battery life.

The flash script's checks, against real stock images from the
[SonixQMK database](https://github.com/SonixQMK/Mechanical-Keyboard-Database)
(`stockFWs/Keychron/240B`) and the images from section 4 plus a K8 image:

    python3 keyboards/keychron/k2/rgb/v2/flash/test_flash_k2.py \
        keyboards/keychron/k2/rgb/v2/flash/flash_k2.py dist <stock image dir>

## 9. Files

| Path | Contents |
|---|---|
| `v2.c` | Bluetooth control: profiles, pairing, battery, indicators, backlight timeout |
| `sleep.c` | Deep sleep on Bluetooth |
| `config.h`, `rules.mk`, `info.json`, `halconf.h`, `mcuconf.h` | Board settings: Bluetooth module, SPI0, keycodes |
| `ansi/`, `iso/` | Layouts and keymaps (`default`, `default_via`) |
| `via_json/` | VIA definitions |
| `flash/` | `flash_k2.py`, `fetch_stock.sh`, `test_flash_k2.py` |
| `release/build_all.sh` | Builds and checks the release images |
| `sim/renode/` | Emulator tests |
