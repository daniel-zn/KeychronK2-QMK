// Copyright 2026 goyamamoto
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// For keymaps (QMK includes this header in every keymap through
// QMK_KEYBOARD_H).

// Call after changing the lighting with the rgb_matrix_*_noeeprom()
// functions: v2.c saves it later, when the LEDs are dark or a minute after
// the last change, since a save blinks the LEDs.
void k2_rgb_changed(void);
