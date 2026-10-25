/* Copyright 2022 Philip Mourdjis <philip.j.m@gmail.com>
 * Copyright 2023 KeijoMika (https://github.com/KeijoMika)
 * Copyright 2023 Santanu Paik (https://github.com/ITNerd69)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#define SN32F2XX_RGB_MATRIX_ROW_PINS { C3, C1, C0, C6, C5, C4, C9, C8, C7, C12, C11, C10, B13, C14, C13, B14, B15, D3 }
// Load each LED row's duty cycles with the PWM timer stopped, then restart it
#define SN32F2XX_PWM_RESTART
#define DIP_SWITCH_PINS { D4, D5 }

// Bluetooth module (ITON) on SPI0, request line A0, direction line A1: the
// same wiring as the K8 RGB (the stock K2 and K8 firmwares drive it with the
// same code)
#define ITON_BT_ENABLE_LED_STATE
#define ITON_BT_SN32_LINE_IRQ

// PAL line callbacks for the module cost 512 bytes of the 8 KiB RAM; shrink the
// EEPROM cache to make room. Without VIA, eeconfig needs well under 256 bytes.
#define WEAR_LEVELING_LOGICAL_SIZE 256
