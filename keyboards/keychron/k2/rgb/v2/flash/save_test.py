"""Checks when the K2 saves its lighting (v2.c saves it later, without a blink).

    uv run --with hidapi python save_test.py <sonixflasher>

Over VIA: a save returns at once, a restart right after a change loses it,
a restart 65 s later keeps it, turning the lights off saves at once. It
restarts the keyboard through the bootloader and back (nothing is flashed)
and leaves the lights off at brightness 180: set them back afterwards.
"""
import subprocess, sys, time, hid
FLASHER = sys.argv[1]
def open_via(wait=20):
    end = time.time() + wait
    while time.time() < end:
        for d in hid.enumerate(0x3434, 0xFE02):
            if d["usage_page"] == 0xFF60 and d["usage"] == 0x61:
                h = hid.device()
                try:
                    h.open_path(d["path"]); h.write(b"\x00\x01" + bytes(31))
                    if len(h.read(32, 1000)) == 32: return h
                except OSError: pass
                h.close()
        time.sleep(1)
    sys.exit("keyboard did not come back")
def cmd(h, *b):
    t = time.perf_counter(); h.write(b"\x00" + bytes(b) + bytes(32 - len(b))); r = bytes(h.read(32, 2000)); return r, (time.perf_counter() - t) * 1000
def state(h):
    return {"brightness": cmd(h, 0x08, 3, 1)[0][3], "effect": cmd(h, 0x08, 3, 2)[0][3]}
def restart(h):
    cmd(h, 0x0B, *b"BOOT"); h.close(); time.sleep(2)
    subprocess.run([FLASHER, "-v", "0c45/7040", "-u"], capture_output=True)
    time.sleep(3)
    return open_via()
ok = True
def check(name, cond, got):
    global ok; ok &= cond; print(("PASS " if cond else "FAIL ") + name, got)
h = open_via(); print("start", state(h))
cmd(h, 0x07, 3, 1, 200); _, ms = cmd(h, 0x09, 3)
check("VIA save no longer writes the flash (~2 ms)", ms < 8, f"{ms:.1f} ms")
h = restart(h); s = state(h)
check("restart right after a change: not saved yet", s["brightness"] == 255, s)
cmd(h, 0x07, 3, 1, 200); cmd(h, 0x09, 3); print("waiting 65 s for the delayed save ..."); time.sleep(65)
h = restart(h); s = state(h)
check("restart 65 s after a change: saved", s["brightness"] == 200, s)
cmd(h, 0x07, 3, 1, 180); cmd(h, 0x07, 3, 2, 0); cmd(h, 0x09, 3); time.sleep(1)
h = restart(h); s = state(h)
check("lights turned off: saved at once (180, off)", s["brightness"] == 180 and s["effect"] == 0, s)
print("RESULT", "pass" if ok else "FAIL")
