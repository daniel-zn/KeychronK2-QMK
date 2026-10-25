#!/usr/bin/env python3
"""Talks VIA's raw HID protocol to the K2 (3434:fe02) to check it without a browser.

    via_test.py check <elf>         protocol, layers, RGB values, keymap == the image's defaults
    via_test.py get <layer> <row> <col>
    via_test.py set <layer> <row> <col> <keycode hex>
    via_test.py reset               load the firmware's default keymap
    via_test.py rgb                 read the RGB matrix values
    via_test.py rgbset <brightness> <effect>   (VIA's Lighting menu: v2.c saves it later)
    via_test.py dump                print the four layers
"""
import pathlib
import struct
import subprocess
import sys

import hid

VID, PID = 0x3434, 0xFE02
ROWS, COLS, LAYERS = 6, 16, 4


def open_via():
    for d in hid.enumerate(VID, PID):
        if d["usage_page"] == 0xFF60 and d["usage"] == 0x61:
            h = hid.device()
            h.open_path(d["path"])
            return h
    sys.exit("no VIA interface (3434:fe02, usage page 0xFF60) found")


def cmd(h, *data):
    pkt = bytes(data) + bytes(32 - len(data))
    h.write(b"\x00" + pkt)
    r = bytes(h.read(32, 1000))
    if len(r) != 32:
        sys.exit(f"no reply to command {data[0]:#04x}")
    if r[0] == 0xFF:
        sys.exit(f"command {data[0]:#04x} unhandled")
    return r


def read_keymap(h):
    size = LAYERS * ROWS * COLS * 2
    buf = b""
    while len(buf) < size:
        n = min(28, size - len(buf))
        r = cmd(h, 0x12, len(buf) >> 8, len(buf) & 0xFF, n)
        buf += r[4:4 + n]
    return struct.unpack(f">{size // 2}H", buf)


def elf_keymap(elf):
    """The compiled default keymap (symbol `keymaps`, little-endian uint16)."""
    nm = subprocess.run(["arm-none-eabi-nm", "-S", elf], capture_output=True, text=True, check=True).stdout
    symbols = (line.split() for line in nm.splitlines())
    addr, size = next((int(f[0], 16), int(f[1], 16)) for f in symbols if len(f) == 4 and f[3] == "keymaps")
    hdrs = subprocess.run(["arm-none-eabi-objdump", "-h", elf], capture_output=True, text=True, check=True).stdout
    for line in hdrs.splitlines():
        f = line.split()
        if len(f) >= 6 and f[0].isdigit():
            vma, off, sz = int(f[3], 16), int(f[5], 16), int(f[2], 16)
            if vma <= addr < vma + sz:
                start = off + addr - vma
                data = pathlib.Path(elf).read_bytes()[start:start + size]
                return struct.unpack(f"<{size // 2}H", data)
    sys.exit("keymaps not found in the elf")


def rgb(h):
    names = {1: "brightness", 2: "effect", 3: "speed", 4: "colour (h,s)"}
    out = {}
    for vid, name in names.items():
        r = cmd(h, 0x08, 3, vid)
        out[name] = tuple(r[3:5]) if vid == 4 else r[3]
    return out


def main():
    a = sys.argv[1:]
    h = open_via()
    if not a or a[0] == "check":
        ok = True
        r = cmd(h, 0x01)
        print(f"VIA protocol {r[1] << 8 | r[2]:#06x}")
        r = cmd(h, 0x02, 0x01)
        print(f"uptime {struct.unpack('>I', r[2:6])[0] / 1000:.1f} s")
        r = cmd(h, 0x11)
        print(f"layers {r[1]}")
        ok &= r[1] == LAYERS
        print(f"macros {cmd(h, 0x0C)[1]}, macro buffer {struct.unpack('>H', cmd(h, 0x0D)[1:3])[0]} bytes")
        print(f"rgb matrix {rgb(h)}")
        if len(a) > 1:
            live, built = read_keymap(h), elf_keymap(a[1])
            diff = [(i // (ROWS * COLS), i // COLS % ROWS, i % COLS, b, l) for i, (l, b) in enumerate(zip(live, built)) if l != b]
            for layer, row, col, b, l in diff:
                print(f"  differs: layer {layer} row {row} col {col}: image {b:#06x}, keyboard {l:#06x}")
            print(f"keymap vs image defaults: {len(live) - len(diff)}/{len(live)} match")
            ok &= not diff
        print("RESULT", "pass" if ok else "FAIL")
        sys.exit(0 if ok else 1)
    if a[0] == "get":
        l, r_, c = map(int, a[1:4])
        r = cmd(h, 0x04, l, r_, c)
        print(f"{r[4] << 8 | r[5]:#06x}")
    elif a[0] == "set":
        l, r_, c = map(int, a[1:4])
        k = int(a[4], 16)
        cmd(h, 0x05, l, r_, c, k >> 8, k & 0xFF)
        r = cmd(h, 0x04, l, r_, c)
        print(f"set {k:#06x}, reads back {r[4] << 8 | r[5]:#06x}")
    elif a[0] == "reset":
        cmd(h, 0x06)
        print("keymap reset to the firmware's defaults")
    elif a[0] == "rgb":
        print(rgb(h))
    elif a[0] == "rgbset":
        # The effect first: with the backlight off, QMK ignores a brightness.
        cmd(h, 0x07, 3, 2, int(a[2]))
        cmd(h, 0x07, 3, 1, int(a[1]))
        cmd(h, 0x09, 3)
        print(rgb(h))
    elif a[0] == "dump":
        km = read_keymap(h)
        for layer in range(LAYERS):
            print(f"layer {layer}")
            for row in range(ROWS):
                i = (layer * ROWS + row) * COLS
                print("  " + " ".join(f"{k:04x}" for k in km[i:i + COLS]))


if __name__ == "__main__":
    main()
