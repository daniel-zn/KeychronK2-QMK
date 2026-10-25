#!/usr/bin/env python3
"""Flash a Keychron K2 v2 RGB (SN32F248B) with SonixFlasherC, after sanity checks.

    ../../../../k8/flash/build_flasher.sh           # once: builds SonixFlasherC
    ./flash_k2.py keychron_k2_rgb_v2_ansi_default_via.bin
    ./flash_k2.py stock/Keychron_K2_RGB_Rev2_ANSI_v1.04_240B.bin   # back to stock

Before anything is written, the script checks that the file is an SN32F248B
image built for a K2 v2 RGB and reads its layout from its USB device
descriptor. It then finds the keyboard (stock firmware, this QMK, or the ROM
bootloader). The K2 and the K8 share the stock USB IDs, and the first K2 RGB
(version 1) has different firmware, so on stock firmware it also checks the
USB strings: a K2 v2 says "Keychron" / "Keychron K2". It refuses a layout or
model mismatch, then asks for confirmation.
"""
import argparse
import pathlib
import re
import struct
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent
# SonixFlasherC with the K8 port's macOS patch, built by k8/flash/build_flasher.sh.
DEFAULT_FLASHER = HERE.parents[3] / "k8" / "flash" / ".flasher" / "sonixflasher"

FLASH_SIZE = 64 * 1024
RAM_START, RAM_END = 0x20000000, 0x20002000

BOOTLOADER = (0x0C45, 0x7040)
LAYOUTS = {
    (0x05AC, 0x024F): ("stock", "ANSI"),
    (0x05AC, 0x0250): ("stock", "ISO"),
    (0x05AC, 0x0251): ("stock", "JIS"),
    (0x3434, 0xFE02): ("QMK", "ANSI"),
    (0x3434, 0xFE03): ("QMK", "ISO"),
}

# USB strings of the K2 v2 RGB's stock firmware (v1.04 ANSI, v1.05 ISO, v1.18
# JIS). The K2 v1 says "Keytron" or "USB DEVICE"; the K8 says "Keychron K8".
STOCK_K2V2_STRINGS = ("Keychron", "Keychron K2")
K2_NAME = "Keychron K2".encode("utf-16le")


def fail(msg):
    sys.exit(f"error: {msg}")


def check_image(path):
    """The image's (VID, PID); exits unless it is a K2 v2 RGB image."""
    try:
        image = path.read_bytes()
    except OSError as e:
        fail(f"cannot read {path}: {e.strerror}")
    if not 1024 <= len(image) <= FLASH_SIZE:
        fail(f"{path.name} is {len(image)} bytes; an SN32F248B image is at most {FLASH_SIZE}")

    sp, reset = struct.unpack_from("<II", image)
    if not RAM_START < sp <= RAM_END:
        fail(f"initial stack pointer {sp:#010x} is outside the 8 KiB RAM")
    if not (reset & 1 and reset < len(image)):
        fail(f"reset vector {reset:#010x} does not point to Thumb code inside the image")

    # QMK says "Keychron K2v2 RGB ANSI", stock says "Keychron K2"; a K8 image
    # says "Keychron K8".
    if K2_NAME not in image:
        fail(f"{path.name} does not name a Keychron K2 in its USB strings; is it a K2 firmware?")
    if b"K\0e\0y\0t\0r\0o\0n\0" in image or b"U\0S\0B\0 \0D\0E\0V\0I\0C\0E\0" in image:
        fail(f"{path.name} is K2 version 1 (Rev1) firmware; this is for the K2 v2 RGB")

    for m in re.finditer(rb"\x12\x01..\x00\x00\x00.(..)(..)", image, re.S):
        ids = tuple(struct.unpack("<H", g)[0] for g in m.groups())
        if ids in LAYOUTS:
            return ids
    fail("no Keychron K2 USB device descriptor in the image; is it a K2 v2 RGB firmware?")


def connected_devices():
    """{(VID, PID): (manufacturer, product)} of the USB devices attached."""
    devices = {}
    if sys.platform == "darwin":
        out = subprocess.run(["ioreg", "-p", "IOUSB", "-l", "-w0"], capture_output=True, text=True).stdout
        for block in out.split("+-o ")[1:]:
            vid = re.search(r'"idVendor" = (\d+)', block)
            pid = re.search(r'"idProduct" = (\d+)', block)
            if not (vid and pid):
                continue
            man = re.search(r'"(?:USB Vendor Name|kUSBVendorString)" = "([^"]*)"', block)
            prod = re.search(r'"(?:USB Product Name|kUSBProductString)" = "([^"]*)"', block)
            devices[(int(vid.group(1)), int(pid.group(1)))] = (man.group(1) if man else None, prod.group(1) if prod else None)
    else:
        for dev in pathlib.Path("/sys/bus/usb/devices").glob("*"):
            try:
                ids = (int((dev / "idVendor").read_text(), 16), int((dev / "idProduct").read_text(), 16))
            except (FileNotFoundError, ValueError):
                continue

            def read(name):
                try:
                    return (dev / name).read_text().strip()
                except (FileNotFoundError, OSError):
                    return None

            devices[ids] = (read("manufacturer"), read("product"))
    return devices


def find_keyboard(devices):
    found = [i for i in devices if i in LAYOUTS or i == BOOTLOADER]
    if len(found) > 1:
        fail(f"more than one keyboard or Sonix bootloader attached: {', '.join(f'{v:04x}:{p:04x}' for v, p in found)}")
    return found[0] if found else None


def check_model(ids, strings):
    """None if the keyboard is a K2 v2 RGB as far as USB tells, else why not."""
    if ids == BOOTLOADER or LAYOUTS[ids][0] == "QMK":
        return None
    if strings == STOCK_K2V2_STRINGS:
        return None
    man, prod = strings
    if prod and "K2" not in prod:
        return f"the keyboard calls itself {prod!r}, not a Keychron K2"
    if man is None or prod is None:
        return "cannot read the keyboard's USB name to tell a K2 v2 from a K2 v1 or a K8"
    return f"the keyboard says {man!r} / {prod!r}; a K2 v2 RGB says {STOCK_K2V2_STRINGS[0]!r} / {STOCK_K2V2_STRINGS[1]!r} (a K2 v1 has other firmware)"


def wait_for(target, seconds):
    deadline = time.time() + seconds
    while time.time() < deadline:
        if target in connected_devices():
            return True
        time.sleep(0.5)
    return False


def describe(ids):
    if ids == BOOTLOADER:
        return "ROM bootloader (0c45:7040)"
    kind, layout = LAYOUTS[ids]
    return f"{kind} firmware, {layout} ({ids[0]:04x}:{ids[1]:04x})"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("image", type=pathlib.Path)
    ap.add_argument("--flasher", type=pathlib.Path, default=DEFAULT_FLASHER)
    ap.add_argument("--yes", action="store_true", help="do not ask for confirmation")
    ap.add_argument("--force", action="store_true", help="flash even if the layout or model checks fail")
    args = ap.parse_args()

    image_ids = check_image(args.image)
    print(f"image:    {args.image.name}: {describe(image_ids)}")

    if not args.flasher.exists():
        fail(f"{args.flasher} not found; run keyboards/keychron/k8/flash/build_flasher.sh first")

    devices = connected_devices()
    keyboard = find_keyboard(devices)
    if keyboard is None:
        fail("no Keychron K2 found; connect it by USB with the side switch on Cable")
    print(f"keyboard: {describe(keyboard)}")

    problem = check_model(keyboard, devices[keyboard])
    if problem:
        if not args.force:
            fail(f"{problem} (use --force to override)")
        print(f"warning: {problem}")

    if keyboard == BOOTLOADER:
        print("note: the bootloader does not tell which keyboard or layout this is; check the image by hand")
    else:
        image_layout, keyboard_layout = LAYOUTS[image_ids][1], LAYOUTS[keyboard][1]
        if image_layout != keyboard_layout and not args.force:
            fail(f"image is for {image_layout} but the keyboard is {keyboard_layout} (use --force to override)")

    if keyboard in LAYOUTS and LAYOUTS[keyboard][0] == "QMK":
        print("Press Fn+Esc on the K2 to enter the bootloader (waiting 30 s)...")
        if not wait_for(BOOTLOADER, 30):
            fail("the ROM bootloader did not appear")
        keyboard = BOOTLOADER
        print(f"keyboard: {describe(keyboard)}")

    if keyboard == BOOTLOADER:
        target = ["-v", "0c45/7040"]
    else:
        # Stock firmware switches to ISP mode on this command and keeps its USB ID.
        target = ["-v", f"{keyboard[0]:04x}/{keyboard[1]:04x}", "-r", "hfd"]

    print("\nKeep the side switch on Cable and the cable plugged in until flashing finishes.")
    if not args.yes:
        try:
            answer = input("Flash now? [y/N] ")
        except EOFError:
            answer = ""
        if answer.strip().lower() != "y":
            sys.exit("aborted")

    cmd = [str(args.flasher), *target, "-f", str(args.image)]
    print("$", " ".join(cmd))
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    output = []
    for line in proc.stdout:
        sys.stdout.write(line)
        output.append(line)
    if proc.wait() != 0 or not any("FLASHING COMPLETED SUCCESSFULLY" in line for line in output):
        fail("flashing did not complete; the ROM bootloader sits outside the flash being written, reach it with the BOOT pad")

    print(f"\nwaiting for {describe(image_ids)} ...")
    if wait_for(image_ids, 15):
        print("done: the keyboard restarted with the new firmware")
    else:
        fail(f"the keyboard did not come back as {image_ids[0]:04x}:{image_ids[1]:04x}; replug it and check")


if __name__ == "__main__":
    main()
