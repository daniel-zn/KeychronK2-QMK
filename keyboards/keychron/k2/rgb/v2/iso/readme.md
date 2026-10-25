# Keychron K2V2 RGB ISO


*A 75% keyboard sold by keychron*

* Keyboard Maintainer: [KeijoMika](https://github.com/KeijoMika) (based on work of dexter93 on a previous branch)
* Hardware Supported: *Keychron K2V2 RGB, MCU: SN32F248BF*
* Hardware Availability: *https://www.keychron.com/products/keychron-k2-hot-swappable-wireless-mechanical-keyboard*

Compile example for this keyboard (after setting up your build environment):

    qmk compile -kb keychron/k2/rgb/v2/iso -km default

Compile example for this keyboard with VIA (after setting up your build environment):

    qmk compile -kb keychron/k2/rgb/v2/iso -km default_via

Wired and Bluetooth (the ITON module, as on the K8). Flashing with `flash/flash_k2.py` (also from stock firmware), the Bluetooth keys and going back to stock are in the [K2 v2 RGB guide](../readme.md).



See the [build environment setup](https://docs.qmk.fm/#/getting_started_build_tools) and the [make instructions](https://docs.qmk.fm/#/getting_started_make_guide) for more information. Brand new to QMK? Start with our [Complete Newbs Guide](https://docs.qmk.fm/#/newbs).

## Bootloader

Enter the bootloader in 2 ways:

* **Physical Bootloader Pins**: Turn off the keyboard, Using a pair of tweezers or a paperclip, short the 2 pins left from spacebar switch holes and hold the tweezers on the pins while turning the keyboard on, this should put it into the bootloader.
* **Keycode in layout**: Press the key mapped to `QK_BOOT` if it is available. In the default keymap Fn+Esc.