#!/usr/bin/env python3
"""Checks when the K2 saves its lighting (v2.c saves it later, without a blink).

    uv run --with hidapi python save_test.py <sonixflasher>

Over VIA: a save returns at once, a restart right after a change loses it, a
restart 65 s later keeps it, turning the backlight off saves at once. It
restarts the keyboard through the ROM bootloader and back (nothing is
flashed), takes about two minutes and sets the lighting back at the end.
"""
import subprocess
import sys
import time

import hid

from via_test import PID, VID

RGB_MATRIX = 3
BRIGHTNESS, EFFECT = 1, 2


def open_via(wait=20):
    """The VIA interface, once the keyboard answers on it."""
    deadline = time.time() + wait
    while time.time() < deadline:
        for d in hid.enumerate(VID, PID):
            if d["usage_page"] != 0xFF60 or d["usage"] != 0x61:
                continue
            h = hid.device()
            try:
                h.open_path(d["path"])
                h.write(b"\x00\x01" + bytes(31))
                if len(h.read(32, 1000)) == 32:
                    return h
            except OSError:
                pass
            h.close()
        time.sleep(1)
    sys.exit("the keyboard did not come back")


def cmd(h, *data):
    """The reply and how long it took, in ms."""
    start = time.perf_counter()
    h.write(b"\x00" + bytes(data) + bytes(32 - len(data)))
    reply = bytes(h.read(32, 2000))
    return reply, (time.perf_counter() - start) * 1000


def lighting(h):
    return {name: cmd(h, 0x08, RGB_MATRIX, value)[0][3] for name, value in (("brightness", BRIGHTNESS), ("effect", EFFECT))}


def set_lighting(h, brightness, effect):
    # The effect first: with the backlight off, QMK ignores a brightness.
    cmd(h, 0x07, RGB_MATRIX, EFFECT, effect)
    cmd(h, 0x07, RGB_MATRIX, BRIGHTNESS, brightness)
    return cmd(h, 0x09, RGB_MATRIX)[1]


def restart(h, flasher):
    """Through the ROM bootloader and back, without flashing."""
    cmd(h, 0x0B, *b"BOOT")
    h.close()
    time.sleep(2)
    subprocess.run([flasher, "-v", "0c45/7040", "-u"], capture_output=True, check=False)
    time.sleep(3)
    return open_via()


def main():
    flasher = sys.argv[1]
    results = []

    def check(name, ok, got):
        results.append(ok)
        print(("PASS " if ok else "FAIL ") + name, got)

    h = open_via()
    before = lighting(h)
    print("start", before)
    test = 200 if before["brightness"] != 200 else 180

    ms = set_lighting(h, test, before["effect"] or 1)
    check("a save returns at once (no flash write)", ms < 8, f"{ms:.1f} ms")
    h = restart(h, flasher)
    got = lighting(h)
    check("restart right after a change: not saved yet", got["brightness"] == before["brightness"], got)

    set_lighting(h, test, before["effect"] or 1)
    print("waiting 65 s for the delayed save ...")
    time.sleep(65)
    h = restart(h, flasher)
    got = lighting(h)
    check("restart 65 s after a change: saved", got["brightness"] == test, got)

    # Brightness while still on, then off.
    cmd(h, 0x07, RGB_MATRIX, BRIGHTNESS, before["brightness"])
    cmd(h, 0x07, RGB_MATRIX, EFFECT, 0)
    cmd(h, 0x09, RGB_MATRIX)
    time.sleep(1)
    h = restart(h, flasher)
    got = lighting(h)
    check("backlight turned off: saved at once", got["effect"] == 0 and got["brightness"] == before["brightness"], got)

    # Back as it was; turning off saved it, turning on saves within a minute.
    set_lighting(h, before["brightness"], before["effect"])
    print("RESULT", "pass" if all(results) else "FAIL")
    sys.exit(0 if all(results) else 1)


if __name__ == "__main__":
    main()
