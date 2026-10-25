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
| Backlight | Keychron's effects | QMK's effects (about 40), set from VIA's Lighting tab. Changes show at once and are saved when the LEDs are dark (off, idle timeout, host asleep) or a minute after the last change: a save stalls the MCU for about 24 ms and blanks the LEDs, a blink on every step if saved at once. Unplugging within that minute loses the change |
| Caps Lock light | Yes | Yes, also over Bluetooth |
| Caps Lock on a Mac | Every tap | Every tap: macOS ignores Caps Lock presses shorter than about 80 ms, so the firmware holds a quick tap for 100 ms (`K2_CAPS_MIN_HOLD`) |
| Fn+Esc | — | Bootloader, for reflashing (the keyboard stops until replugged if pressed by accident). VIA images also enter it on request over USB, which `flash_k2.py` uses, so the key can be remapped in VIA with nothing lost |

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
| Fits | VIA images use flash up to 0xE424 of 0xF7C0 (settings area above; Arm's macOS GCC 13.2.1 build, the Linux one adds about 450 bytes), 208–224 bytes RAM free (the hardware-tested K8 VIA image has 224) |
| Emulator tests | 61 checks pass, 0 fail, 1 known issue (the Caps Lock one above), see [Tests](#8-tests) |
| Flash script checks | Refuses K8 images, K2 v1 images, junk, layout mismatches, a stock keyboard that is not a K2 v2; asks for the bootloader over USB and falls back to Fn+Esc (24 checks in `flash/test_flash_k2.py`) |
| Reproducible | A clean rebuild with the same compiler gives byte-identical images. The VIA images depend on the build date (VIA's settings check uses it), so a rebuild on another day differs. Arm's macOS build of GCC 13.2.1 (Homebrew `osx-cross/arm/arm-gcc-bin@13`) makes images about 450 bytes smaller than the Linux one, with the same free RAM |

**On a real K2** (ANSI, stock firmware v1.07, Mac mini and MacBook, 2026-10-25):

| Check | Result |
|---|---|
| Board check: USB "Keychron" / "Keychron K2", 05ac:024f, bcdDevice 0x0107, RGB backlight | Pass |
| First flash from stock v1.07 with the `hfd` ISP command, case closed | Pass: flashed and verified, the keyboard came back as 3434:fe02 |
| Reflash: Fn+Esc enters the ROM bootloader (0c45:7040), `flash_k2.py` flashes it | Pass |
| Saving settings without a flash of light: lighting changes in quick succession and Fn+Light off/on, after the LED driver goes dark during flash writes (before, a random row flashed at full brightness on each save) | Pass |
| Hold Fn+F5 / Fn+F6 to keep dimming or brightening | Pass |
| Caps Lock tapped quickly and repeatedly on a Mac registers every time (before: some quick taps did nothing) | Pass |
| Lighting saved later, no blink per step: brightness steps and effect changes | Pass by eye; over USB (`flash/save_test.py`) a VIA save takes 2 ms instead of 24, a restart right after a change loses it, 65 s after keeps it, turning off saves at once. Saving when the host sleeps or the Bluetooth idle timeout turns the backlight off: not tried |
| Reflash with no key press: `flash_k2.py` asks over USB, the firmware enters the ROM bootloader, flashes, comes back | Pass (twice, with a userspace keymap built on this firmware's `v2.c`); a firmware without the request answers "unhandled" and the script falls back to Fn+Esc, also checked on the keyboard |
| Typing, wired | Pass (most keys checked by hand, on another computer) |
| RGB effects, no flicker | Pass |
| Fn row, Fn+arrows, Caps Lock light, wired | Mostly checked, nothing failed |
| VIA over raw HID: protocol 0x000D, 4 layers, 16 macros, RGB values, all 384 keys match the image | Pass (by script, not in usevia.app) |
| VIA remap: Right Ctrl set to A types "a" and survives unplugging | Pass |
| Bluetooth: hosts paired under stock still connect and type | Pass |
| Bluetooth: new pairing with a MacBook, typing | Pass |
| VIA in usevia.app (Design tab, Custom keys, Lighting menu) | Not tried |
| Fn+B battery level, Fn+1/2/3 profile switching, Cable → BT → Cable | Not tried |
| Backlight off after 5 min, deep sleep after 10 min, wake keystroke | Not tried |
| Caps Lock light over Bluetooth | Not tried |
| Back to stock | Not tried (and only v1.04 exists to go back to; see [Back to stock](#7-back-to-stock)) |

It cannot brick the keyboard: the SN32F248B's bootloader is in ROM, so
shorting the two BOOT pads left of the spacebar switch while plugging in
always gets back to a flashable state, and stock firmware can be put back.

## 4. Build

macOS (tested on Apple silicon, 2026-10): the ARM compiler from the
osx-cross tap, the `qmk` CLI from PyPI, and libusb for the flasher:

    brew tap osx-cross/arm && brew trust osx-cross/arm     # newer Homebrew asks to trust a tap
    brew install osx-cross/arm/arm-gcc-bin@13 libusb pkgconf
    uv tool install --python 3.12 qmk                      # or: pipx install qmk
    export PATH="$(brew --prefix)/opt/arm-gcc-bin@13/bin:$PATH"   # keg-only

(QMK's own `brew install qmk/qmk/qmk` stops at untrusted taps, `qmk/qmk`,
`osx-cross/arm` and then `osx-cross/avr`, and brings the AVR toolchain this
keyboard does not need.) Debian/Ubuntu: `apt install gcc-arm-none-eabi libnewlib-arm-none-eabi`
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

    uv run --with hidapi python keyboards/keychron/k2/rgb/v2/flash/flash_k2.py dist/keychron_k2_rgb_v2_ansi_default_via.bin

(`hidapi` lets the script ask a keyboard on this firmware for the bootloader
over USB; `pip install hidapi` and plain `flash_k2.py ...` work as well, and
without it the script asks for Fn+Esc.)

| The keyboard runs | What happens |
|---|---|
| Stock firmware (first time) | The script sends the ISP command stock firmware accepts, so there is no need to open the case |
| This firmware, a VIA image (later) | The script asks the firmware over USB to enter the bootloader: no key press |
| This firmware, a non-VIA image, or a VIA image built before the USB request was added | Press **Fn+Esc** when the script asks |
| The ROM bootloader | Flashed directly |

The script refuses an image that is not for a K2 v2 RGB, a layout mismatch,
and a stock keyboard whose USB strings are not a K2 v2's ("Keychron" /
"Keychron K2"; a K2 version 1 or a K8 says something else). It asks before
writing and waits for the keyboard to come back.

Every flash writes the whole 64 KiB, settings area included, so it resets
what VIA saved (remapped keys, macros, lighting) and the Bluetooth profile
number; pairings live in the Bluetooth module and stay. Save the layout from
VIA's Save + Load tab before reflashing, and load it again afterwards.

The stock firmware on the tested board was v1.07, newer than the v1.04 the
ISP command was found in; it took the command all the same.

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
| Light (top right) | Next lighting effect; Fn+Light turns the lights on or off |
| Fn+F5 / Fn+F6 | Backlight dimmer / brighter; hold to keep going (also Fn+arrows for hue and saturation) |

Moving the side switch restarts the keyboard. To pair: switch to BT, hold
Fn+1 until the 1 key blinks blue, connect to "Keychron K2" from the host.
White blinking is reconnecting; green or red after an action is connected or
not. Pairings made under stock firmware are kept by the module and may still
work.

**VIA:** open [usevia.app](https://usevia.app) in Chrome. In Settings, turn on
"Show Design tab". In Design, load `via_json/k2_ansi.json` (or `k2_iso.json`),
then remap in Configure. The Bluetooth keys are under Custom: Batt, BT1, BT2,
BT3.

### Your own keymap, kept outside this repository

VIA changes keys without rebuilding, but every flash resets them (see
[Flash](#5-flash)). A layout that should survive reflashing, or one that
changes more than keys (which lighting effects are built in, new keys), can
live in a [QMK userspace](https://docs.qmk.fm/newbs_external_userspace): a
folder of your own with this layout,

    qmk.json                                  {"userspace_version": "1.0", "build_targets": []}
    keyboards/keychron/k2/rgb/v2/ansi/keymaps/<name>/keymap.c
    keyboards/keychron/k2/rgb/v2/ansi/keymaps/<name>/rules.mk     VIA_ENABLE = yes
    keyboards/keychron/k2/rgb/v2/ansi/keymaps/<name>/config.h     the two lines of default_via/config.h, plus your own

Start `keymap.c` from `ansi/keymaps/default/keymap.c`. Build it from this
repository (plain `make` does not look in a userspace) with

    QMK_HOME=$PWD QMK_USERSPACE=/path/to/your/folder qmk compile -kb keychron/k2/rgb/v2/ansi -km <name>

and flash the `.bin` with `flash_k2.py` as above. Turning effects off in
`config.h` (`#undef ENABLE_RGB_MATRIX_<EFFECT>`) renumbers the rest, so VIA's
own Lighting menu then names the wrong ones: give VIA a copy of
`via_json/k2_ansi.json` whose `menus` lists your effects by number (0 is off,
1 solid colour, then the enabled ones in the order of
`quantum/rgb_matrix/animations/rgb_matrix_effects.inc`).

## 7. Back to stock

    keyboards/keychron/k2/rgb/v2/flash/fetch_stock.sh
    keyboards/keychron/k2/rgb/v2/flash/flash_k2.py keyboards/keychron/k2/rgb/v2/flash/stock/Keychron_K2_RGB_Rev2_ANSI_v1.04_240B.bin

Keychron's program from the SonixQMK database, not a copy of what was on the
keyboard; ISO: `..._ISO_v1.05_240B.bin`. A keyboard that shipped with a newer
version (the tested one had v1.07, bcdDevice 0x0107) goes back to v1.04: the
flasher cannot read the old firmware out, and no newer image is published.

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

On the keyboard, after flashing a VIA image, `flash/via_test.py` talks VIA's
raw HID protocol (no browser) and checks that the keymap the keyboard holds is
the image's default (Python with `hidapi`, the ARM binutils on the PATH):

    uv run --with hidapi python keyboards/keychron/k2/rgb/v2/flash/via_test.py \
        check .build/keychron_k2_rgb_v2_ansi_default_via.elf

It also reads and sets single keys (`get`, `set`), the lighting (`rgb`,
`rgbset`), dumps the layers (`dump`) and loads the image's defaults (`reset`).

## 9. Files

| Path | Contents |
|---|---|
| `drivers/led/sn32f2xx.c` (from the repository root, shared by all SN32 keyboards) | LED driver: dark while the flash is written |
| `v2.c` | Bluetooth control: profiles, pairing, battery, indicators, backlight timeout; the bootloader on request over USB (VIA images: raw HID command `0x0B` followed by `BOOT`) |
| `sleep.c` | Deep sleep on Bluetooth |
| `config.h`, `rules.mk`, `info.json`, `halconf.h`, `mcuconf.h` | Board settings: Bluetooth module, SPI0, keycodes |
| `ansi/`, `iso/` | Layouts and keymaps (`default`, `default_via`) |
| `via_json/` | VIA definitions |
| `flash/` | `flash_k2.py`, `fetch_stock.sh`, `test_flash_k2.py`, `via_test.py`, `save_test.py` |
| `release/build_all.sh` | Builds and checks the release images |
| `sim/renode/` | Emulator tests |
