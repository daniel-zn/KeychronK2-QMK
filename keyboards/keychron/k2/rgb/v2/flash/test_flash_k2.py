"""Checks flash_k2.py's image and keyboard checks against real images.

    python3 test_flash_k2.py <flash_k2.py> <dir with the four K2 QMK images and the K8 QMK image> <stock image dir>

The stock dir is stockFWs/Keychron/240B of github.com/SonixQMK/Mechanical-Keyboard-Database.
"""
import sys, pathlib, importlib.util, tempfile
spec = importlib.util.spec_from_file_location("flash_k2", sys.argv[1])
fk = importlib.util.module_from_spec(spec); spec.loader.exec_module(fk)
root = pathlib.Path(sys.argv[2]); stock = pathlib.Path(sys.argv[3])
ok = 0; bad = 0
def expect(name, cond):
    global ok, bad
    print(("PASS " if cond else "FAIL ") + name); ok += cond; bad += (not cond)
def image(p):
    try:
        return fk.check_image(p), None
    except SystemExit as e:
        return None, str(e)
for n, want in [("keychron_k2_rgb_v2_ansi_default.bin", (0x3434, 0xFE02)), ("keychron_k2_rgb_v2_ansi_default_via.bin", (0x3434, 0xFE02)),
                ("keychron_k2_rgb_v2_iso_default.bin", (0x3434, 0xFE03)), ("keychron_k2_rgb_v2_iso_default_via.bin", (0x3434, 0xFE03))]:
    ids, err = image(root / n); expect(f"QMK image {n} accepted as {want}", ids == want)
for n, want in [("Keychron_K2_RGB_Rev2_ANSI_v1.04_240B.bin", (0x05AC, 0x024F)), ("Keychron_K2_RGB_Rev2_ISO_v1.05_240B.bin", (0x05AC, 0x0250))]:
    ids, err = image(stock / n); expect(f"stock image {n} accepted", ids == want)
for n in ["Keychron_K2_RGB_Rev1_ANSI_v1.62_240B.bin", "Keychron_K2_RGB_Rev1_ISO_v1.03_240B.bin"]:
    ids, err = image(stock / n); expect(f"K2 v1 image {n} refused ({err})", ids is None and "version 1" in err)
for p in [root / "keychron_k8_rgb_ansi_ansi_via.bin", stock / "Keychron_K8_RGB_ANSI_v1.06_240B.bin"]:
    ids, err = image(p); expect(f"K8 image {p.name} refused", ids is None and "K2" in err)
junk = pathlib.Path(tempfile.mkdtemp()) / "junk.bin"; junk.write_bytes(b"\x00" * 4096)
ids, err = image(junk); expect("junk image refused", ids is None)
# model checks
S = (0x05AC, 0x024F)
expect("stock K2 v2 strings accepted", fk.check_model(S, ("Keychron", "Keychron K2")) is None)
expect("stock K8 refused", fk.check_model(S, ("Keychron K8", "Keychron K8")) is not None)
expect("stock K2 v1 'Keytron' refused", fk.check_model(S, ("Keytron", "Keychron K2")) is not None)
expect("stock K2 v1 'USB DEVICE' refused", fk.check_model(S, ("USB Keychron K2", "USB DEVICE")) is not None)
expect("unreadable strings refused", fk.check_model(S, (None, None)) is not None)
expect("QMK K2 accepted", fk.check_model((0x3434, 0xFE02), (None, None)) is None)
expect("bootloader accepted", fk.check_model(fk.BOOTLOADER, (None, None)) is None)
# find_keyboard
expect("finds the one keyboard", fk.find_keyboard({S: ("Keychron", "Keychron K2"), (1, 2): (None, None)}) == S)
try:
    fk.find_keyboard({S: (None, None), fk.BOOTLOADER: (None, None)}); expect("two keyboards refused", False)
except SystemExit:
    expect("two keyboards refused", True)
print(f"RESULT pass={ok} fail={bad}"); sys.exit(1 if bad else 0)
