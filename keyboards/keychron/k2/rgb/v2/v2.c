// Copyright 2026 goyamamoto
// SPDX-License-Identifier: GPL-2.0-or-later

// Bluetooth for the Keychron K2 v2 RGB, ported from the Keychron K8 RGB
// (goyamamoto/KeychronK8-QMK keyboards/keychron/k8/rgb/rgb.c): the two boards
// share the MCU, the ITON module and its wiring, the side switches and the
// LED rows.

#include QMK_KEYBOARD_H

// libc rand() keeps its state in newlib's reentrancy block, which pulls in
// the stdio stream table: about 400 bytes of the 8 KiB RAM for a random
// number. This replaces it for send_string and the RGB effects.
static uint32_t k2_rand_state = 1;

void srand(unsigned int seed) {
    k2_rand_state = seed ? seed : 1;
}

int rand(void) {
    // xorshift32
    k2_rand_state ^= k2_rand_state << 13;
    k2_rand_state ^= k2_rand_state >> 17;
    k2_rand_state ^= k2_rand_state << 5;
    return (int)(k2_rand_state & RAND_MAX);
}

#ifdef VIA_ENABLE
#    include <string.h>
#    include "raw_hid.h"
#    include "via.h"

// The bootloader on request over USB, so flashing needs no key press:
// flash/flash_k2.py sends VIA's old id_bootloader_jump (0x0B), which today's
// VIA no longer handles, followed by "BOOT" so nothing else triggers it. The
// reply goes out first and the jump follows a moment later.
#    define K2_USB_BOOT_COMMAND 0x0B
#    define K2_USB_BOOT_DELAY 50

static bool     usb_boot_requested;
static uint16_t usb_boot_timer;

bool via_command_kb(uint8_t *data, uint8_t length) {
    // VIA saves the lighting after each change in its Lighting menu; save it
    // later instead, without a blink (k2_rgb_changed()).
    if (data[0] == id_custom_save && data[1] == id_qmk_rgb_matrix_channel) {
        k2_rgb_changed();
        raw_hid_send(data, length);
        return true;
    }
    if (data[0] != K2_USB_BOOT_COMMAND || memcmp(&data[1], "BOOT", 4) != 0) {
        return false;
    }
    raw_hid_send(data, length);
    usb_boot_requested = true;
    usb_boot_timer     = timer_read();
    return true;
}

static void usb_boot_task(void) {
    if (usb_boot_requested && timer_elapsed(usb_boot_timer) > K2_USB_BOOT_DELAY) {
        reset_keyboard();
    }
}
#endif

#ifdef BLUETOOTH_ENABLE
#    include "connection.h"
#    include "iton_bt.h"
#    include "sleep.h"

// Indices into DIP_SWITCH_PINS. Active means Mac and Cable: the stock K2 and
// K8 firmwares read D4 and D5 with the same code, and the K8 port was tested
// on the hardware.
#    define DIP_OS 0
#    define DIP_CONNECTION 1

// Holding Fn+1/2/3 this long puts that profile into pairing.
#    ifndef K2_BT_PAIR_HOLD_TIME
#        define K2_BT_PAIR_HOLD_TIME 3000
#    endif

// Backlight goes dark after this much idle time on Bluetooth to save battery.
#    ifndef K2_BT_RGB_TIMEOUT
#        define K2_BT_RGB_TIMEOUT 300000
#    endif

// The MCU sleeps after this much idle time on Bluetooth; any key wakes it.
// 0 turns sleep off.
#    ifndef K2_BT_SLEEP_TIMEOUT
#        define K2_BT_SLEEP_TIMEOUT 600000
#    endif

// Give the module time to boot before the first command.
#    define K2_BT_STARTUP_DELAY 500
#    define K2_BT_OS_DELAY 300
#    define K2_BT_EVENT_TIME 3000

#    define K2_BT_PROFILES 3

typedef union {
    uint32_t raw;
    struct {
        uint8_t profile;
    };
} k2_config_t;

typedef enum {
    BT_IDLE,
    BT_CONNECTING,
    BT_PAIRING,
    BT_CONNECTED,
    BT_DISCONNECTED,
} bt_state_t;

static k2_config_t k2_config;

static bool     bt_mode;
static bool     mac_mode;
static bool     module_needs_mode = true;
static uint16_t startup_timer;

static bool     pair_hold_active;
static uint16_t pair_hold_timer;

static bool rgb_idle_off;

// Written from the module's IRQ callbacks.
static volatile bt_state_t bt_state;
static volatile uint32_t   bt_state_timer;
static volatile uint8_t    battery_level;
static volatile uint32_t   battery_timer;
static volatile bool       battery_low;

static void set_bt_state(bt_state_t state) {
    bt_state       = state;
    bt_state_timer = timer_read32();
}

void iton_bt_enters_connection_state(void) {
    set_bt_state(BT_CONNECTING);
}

void iton_bt_entered_pairing(void) {
    set_bt_state(BT_PAIRING);
}

// Connected and disconnected show only as the outcome of something the user
// did (starting up, the side switch, switching or pairing a profile); the
// module also reports links that drop and come back on their own.
#    define K2_BT_OUTCOME_TIME 30000

static volatile bool     link_action;
static volatile uint32_t link_action_timer;
static volatile bool     link_outcome_shown;

static void link_action_started(void) {
    link_action       = true;
    link_action_timer = timer_read32();
}

static void set_link_outcome(bt_state_t state) {
    link_outcome_shown = bt_state == BT_PAIRING || (link_action && timer_elapsed32(link_action_timer) < K2_BT_OUTCOME_TIME);
    set_bt_state(state);
}

void iton_bt_connection_successful(void) {
    set_link_outcome(BT_CONNECTED);
}

void iton_bt_disconnected(void) {
    set_link_outcome(BT_DISCONNECTED);
}

// The module also reports the level on its own; show it only as the answer
// to Fn+B.
static volatile bool     battery_asked;
static volatile uint32_t battery_ask_timer;

void iton_bt_battery_level(uint8_t level) {
    if (!battery_asked || timer_elapsed32(battery_ask_timer) > K2_BT_EVENT_TIME) {
        return;
    }
    battery_asked = false;
    battery_level = level;
    battery_timer = timer_read32();
}

void iton_bt_battery_voltage_low(void) {
    battery_low = true;
}

void iton_bt_battery_exit_low_battery_mode(void) {
    battery_low = false;
}

static bool     os_pending;
static uint16_t os_timer;

static void send_mode_to_module(void) {
    link_action_started();
    if (bt_mode) {
        iton_bt_switch_profile(k2_config.profile);
        // The module ignores packets for a while after a profile switch.
        os_pending = true;
        os_timer   = timer_read();
    } else {
        iton_bt_mode_usb();
    }
}

static void send_os_to_module(void) {
    if (mac_mode) {
        iton_bt_os_mac();
    } else {
        iton_bt_os_win();
    }
}

static void select_profile(uint8_t profile) {
    link_action_started();
    if (k2_config.profile != profile) {
        k2_config.profile = profile;
        eeconfig_update_kb(k2_config.raw);
    }
    iton_bt_switch_profile(profile);
}

void eeconfig_init_kb(void) {
    k2_config.raw = 0;
    eeconfig_update_kb(k2_config.raw);
    eeconfig_init_user();
}

void keyboard_post_init_kb(void) {
    k2_config.raw = eeconfig_read_kb();
    if (k2_config.profile >= K2_BT_PROFILES) {
        k2_config.profile = 0;
    }
    startup_timer = timer_read();
    keyboard_post_init_user();
}

// macOS ignores a Caps Lock press shorter than about 80 ms (its Caps Lock
// delay; QMK's TAP_HOLD_CAPS_DELAY is for the same reason), so a quick tap
// sometimes did nothing. The release of a short tap is held back until the
// key has been down K2_CAPS_MIN_HOLD; holding it longer is unchanged.
#    ifndef K2_CAPS_MIN_HOLD
#        define K2_CAPS_MIN_HOLD 100
#    endif

static bool     caps_release_pending;
static uint16_t caps_press_timer;

static bool process_caps(uint16_t keycode, keyrecord_t *record) {
    if (keycode != KC_CAPS) {
        return true;
    }
    if (record->event.pressed) {
        if (caps_release_pending) {
            caps_release_pending = false;
            unregister_code(KC_CAPS);
        }
        caps_press_timer = timer_read();
        return true;
    }
    if (timer_elapsed(caps_press_timer) < K2_CAPS_MIN_HOLD) {
        caps_release_pending = true;
        return false;
    }
    return true;
}

static void caps_release_task(void) {
    if (caps_release_pending && timer_elapsed(caps_press_timer) >= K2_CAPS_MIN_HOLD) {
        caps_release_pending = false;
        unregister_code(KC_CAPS);
    }
}

// Saving the lighting writes the flash: the MCU stalls about 24 ms per save
// and the LEDs go dark meanwhile (drivers/led/sn32f2xx.c), a visible blink on
// every brightness step or effect change. So the lighting keys, VIA's
// Lighting menu and a keymap's own light keys (calling k2_rgb_changed())
// change the lighting in RAM only, and it is saved once the LEDs are dark
// anyway (backlight off, idle timeout, host asleep), when the connection
// switch moves, or K2_RGB_SAVE_DELAY after the last change: one blink, and
// a change survives unplugging or the switch's Off position.
#    ifndef K2_RGB_SAVE_DELAY
#        define K2_RGB_SAVE_DELAY 5000
#    endif

static bool     rgb_save_pending;
static uint32_t rgb_save_timer;

void k2_rgb_changed(void) {
    rgb_save_pending = true;
    rgb_save_timer   = timer_read32();
}

static void rgb_save(void) {
    rgb_config_t config = rgb_matrix_config;
    // Off for the idle timeout only: save the backlight as on.
    if (rgb_idle_off) {
        config.enable = 1;
    }
    eeconfig_update_rgb_matrix(&config);
    rgb_save_pending = false;
}

static void rgb_save_task(void) {
    if (rgb_save_pending && (!rgb_matrix_is_enabled() || timer_elapsed32(rgb_save_timer) > K2_RGB_SAVE_DELAY)) {
        rgb_save();
    }
}

void suspend_power_down_kb(void) {
    if (rgb_save_pending) {
        rgb_save();
    }
    suspend_power_down_user();
}

// Brightness, hue, saturation and speed: one step on press, then repeated
// while held (after K2_RGB_REPEAT_DELAY, every K2_RGB_REPEAT_INTERVAL).
#    ifndef K2_RGB_REPEAT_DELAY
#        define K2_RGB_REPEAT_DELAY 400
#    endif
#    ifndef K2_RGB_REPEAT_INTERVAL
#        define K2_RGB_REPEAT_INTERVAL 80
#    endif

static uint16_t rgb_repeat_keycode;
static bool     rgb_repeating;
static uint16_t rgb_repeat_timer;

// One step of an adjusting key; false if the key is not one. Shift reverses.
static bool rgb_adjust(uint16_t keycode) {
    bool up = !(get_mods() & MOD_MASK_SHIFT);
    switch (keycode) {
        case RM_HUEU:
        case RM_HUED:
            if ((keycode == RM_HUEU) == up) {
                rgb_matrix_increase_hue_noeeprom();
            } else {
                rgb_matrix_decrease_hue_noeeprom();
            }
            break;
        case RM_SATU:
        case RM_SATD:
            if ((keycode == RM_SATU) == up) {
                rgb_matrix_increase_sat_noeeprom();
            } else {
                rgb_matrix_decrease_sat_noeeprom();
            }
            break;
        case RM_VALU:
        case RM_VALD:
            if ((keycode == RM_VALU) == up) {
                rgb_matrix_increase_val_noeeprom();
            } else {
                rgb_matrix_decrease_val_noeeprom();
            }
            break;
        case RM_SPDU:
        case RM_SPDD:
            if ((keycode == RM_SPDU) == up) {
                rgb_matrix_increase_speed_noeeprom();
            } else {
                rgb_matrix_decrease_speed_noeeprom();
            }
            break;
        default:
            return false;
    }
    k2_rgb_changed();
    return true;
}

static void rgb_repeat_task(void) {
    if (rgb_repeat_keycode && timer_elapsed(rgb_repeat_timer) > (rgb_repeating ? K2_RGB_REPEAT_INTERVAL : K2_RGB_REPEAT_DELAY)) {
        rgb_adjust(rgb_repeat_keycode);
        rgb_repeating    = true;
        rgb_repeat_timer = timer_read();
    }
}

// QMK's lighting keys, saved later. The adjusting ones act on press and
// repeat; the others act on release as in process_rgb_matrix() (on press
// with RGB_TRIGGER_ON_KEYDOWN).
static bool process_rgb_key(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed && rgb_adjust(keycode)) {
        rgb_repeat_keycode = keycode;
        rgb_repeating      = false;
        rgb_repeat_timer   = timer_read();
        return false;
    }
    switch (keycode) {
        case RM_HUEU ... RM_SPDD: // the adjusting keys, released
            if (keycode == rgb_repeat_keycode) {
                rgb_repeat_keycode = 0;
            }
            return false;
        case RM_ON:
        case RM_OFF:
        case RM_TOGG:
        case RM_NEXT:
        case RM_PREV:
            break;
        default:
            return true;
    }
#    ifdef RGB_TRIGGER_ON_KEYDOWN
    bool act = record->event.pressed;
#    else
    bool act = !record->event.pressed;
#    endif
    if (!act) {
        return false;
    }
    bool shifted = get_mods() & MOD_MASK_SHIFT;
    switch (keycode) {
        case RM_ON:
            rgb_matrix_enable_noeeprom();
            break;
        case RM_OFF:
            rgb_matrix_disable_noeeprom();
            break;
        case RM_TOGG:
            rgb_matrix_toggle_noeeprom();
            break;
        case RM_NEXT:
        case RM_PREV:
            if ((keycode == RM_NEXT) != shifted) {
                rgb_matrix_step_noeeprom();
            } else {
                rgb_matrix_step_reverse_noeeprom();
            }
            break;
        default:
            return true;
    }
    k2_rgb_changed();
    return false;
}

bool dip_switch_update_kb(uint8_t index, bool active) {
    switch (index) {
        case DIP_OS:
            mac_mode          = active;
            module_needs_mode = true;
            break;
        case DIP_CONNECTION:
            // The switch passes through Off, which restarts the keyboard
            // when nothing else powers it: save the lighting now, while
            // there is still power.
            if (rgb_save_pending) {
                rgb_save();
            }
            bt_mode = !active;
            connection_set_host_noeeprom(bt_mode ? CONNECTION_HOST_BLUETOOTH : CONNECTION_HOST_USB);
            module_needs_mode = true;
            pair_hold_active  = false;
            set_bt_state(BT_IDLE);
            break;
    }
    return dip_switch_update_user(index, active);
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (!process_record_user(keycode, record)) {
        return false;
    }
    if (!process_rgb_key(keycode, record)) {
        return false;
    }
    if (!process_caps(keycode, record)) {
        return false;
    }

    // K2_BT1-3 do what QMK's BT_PRF1-3 do; as keyboard keycodes they show
    // in VIA's key picker by name.
    uint8_t profile;
    switch (keycode) {
        case BT_PRF1 ... BT_PRF3:
            profile = keycode - BT_PRF1;
            break;
        case K2_BT1 ... K2_BT3:
            profile = keycode - K2_BT1;
            break;
        default:
            profile = K2_BT_PROFILES;
            break;
    }

    switch (keycode) {
        case BT_PRF1 ... BT_PRF3:
        case K2_BT1 ... K2_BT3:
            if (!bt_mode) {
                return false;
            }
            if (record->event.pressed) {
                select_profile(profile);
                pair_hold_active = true;
                pair_hold_timer  = timer_read();
            } else {
                pair_hold_active = false;
            }
            return false;
        case K2_BATT:
            if (bt_mode && record->event.pressed) {
                battery_asked     = true;
                battery_ask_timer = timer_read32();
                iton_bt_query_battery_level();
            }
            return false;
    }
    return true;
}

static void rgb_idle_task(void) {
    bool idle = bt_mode && last_input_activity_elapsed() > K2_BT_RGB_TIMEOUT;

    if (idle && !rgb_idle_off && rgb_matrix_is_enabled()) {
        rgb_matrix_disable_noeeprom();
        rgb_idle_off = true;
    } else if (!idle && rgb_idle_off) {
        rgb_matrix_enable_noeeprom();
        rgb_idle_off = false;
    }
}

static k2_sleep_blocker_t sleep_blocker;

static void sleep_task(void) {
    sleep_blocker = K2_SLEEP_OK;
    // Not while pairing or reconnecting: the module's "connected" notice
    // would arrive while the MCU sleeps and be lost, leaving the profile key
    // blinking after the link is up.
    if (bt_state == BT_PAIRING || bt_state == BT_CONNECTING) {
        return;
    }
    if (K2_BT_SLEEP_TIMEOUT && bt_mode && last_input_activity_elapsed() > K2_BT_SLEEP_TIMEOUT) {
        sleep_blocker = k2_sleep_blocker();
        if (sleep_blocker == K2_SLEEP_OK) {
            k2_sleep();
        }
    }
}

#    ifdef ITON_BT_DEBUG
#        include "print.h"

// Print link events as they arrive, plus switch state changes.
static void debug_task(void) {
    static uint8_t printed;
    static int8_t  last_bt = -1;

    // Give the host time to reattach the console after a reset.
    if (timer_read32() < 3000) {
        return;
    }

    if (last_bt != bt_mode) {
        last_bt = bt_mode;
        uprintf("%5u dip bt=%d mac=%d\n", timer_read(), bt_mode, mac_mode);
    }
    while (printed != iton_bt_log_head) {
        if ((uint8_t)(iton_bt_log_head - printed) > ITON_BT_LOG_LEN) {
            uprintf("... %u events lost\n", (uint8_t)(iton_bt_log_head - printed) - ITON_BT_LOG_LEN);
            printed = iton_bt_log_head - ITON_BT_LOG_LEN;
        }
        volatile iton_bt_event_t *e = &iton_bt_log[printed % ITON_BT_LOG_LEN];
        uprintf("%5u %c %02X %02X %02X\n", e->time, e->type, e->data[0], e->data[1], e->data[2]);
        printed++;
    }
}
#    endif

// Keys the indicators use, as groups.
enum {
    LIT_NUMBER_ROW = 1, // profile key, battery level
    LIT_ESC        = 4, // low battery, sleep diagnostics
};

static bool showing(uint32_t since) {
    return timer_elapsed32(since) < K2_BT_EVENT_TIME;
}

static bool link_outcome_showing(void) {
    return link_outcome_shown && (bt_state == BT_CONNECTED || bt_state == BT_DISCONNECTED) && showing(bt_state_timer);
}

static bool battery_showing(void) {
    return battery_level && showing(battery_timer);
}

static uint8_t indicators_lit(void) {
    uint8_t lit = 0;
    if (bt_mode) {
        if (bt_state == BT_PAIRING || bt_state == BT_CONNECTING || link_outcome_showing() || battery_showing()) lit |= LIT_NUMBER_ROW;
        if (battery_low) lit |= LIT_ESC;
    }
#    ifdef K2_SLEEP_DIAG
    if (sleep_blocker != K2_SLEEP_OK) lit |= LIT_ESC;
#    endif
    return lit;
}

// Some effects (raindrops, pixel rain) repaint only a few keys at a time, so
// a key an indicator lit kept its colour long after the indicator ended. When
// one ends, the effect starts again, which repaints every key at once.
static uint8_t lit_before;

static void indicator_end_task(void) {
    uint8_t lit   = indicators_lit();
    uint8_t ended = lit_before & ~lit;
    lit_before    = lit;
    if (ended && rgb_matrix_is_enabled()) {
        rgb_matrix_mode_noeeprom(rgb_matrix_get_mode());
    }
}

// With the backlight off QMK draws no indicators and sends the LEDs one black
// frame only, so the indicators (this file's and a keymap's
// rgb_matrix_indicators_advanced_user()) are drawn over black and sent here.
static void indicators_while_off_task(void) {
    if (rgb_matrix_is_enabled()) {
        return;
    }
    rgb_matrix_set_color_all(RGB_OFF);
    rgb_matrix_indicators_advanced_kb(0, RGB_MATRIX_LED_COUNT);
    rgb_matrix_update_pwm_buffers(); // copies only what changed
}

void housekeeping_task_kb(void) {
    if (module_needs_mode && timer_elapsed(startup_timer) > K2_BT_STARTUP_DELAY) {
        module_needs_mode = false;
        send_mode_to_module();
    }

    if (os_pending && timer_elapsed(os_timer) > K2_BT_OS_DELAY) {
        os_pending = false;
        send_os_to_module();
    }

    if (pair_hold_active && timer_elapsed(pair_hold_timer) > K2_BT_PAIR_HOLD_TIME) {
        pair_hold_active = false;
        link_action_started();
        iton_bt_enter_pairing();
    }

#    ifdef VIA_ENABLE
    usb_boot_task();
#    endif
    rgb_repeat_task();
    caps_release_task();
    rgb_save_task();
    rgb_idle_task();
    sleep_task();
    indicator_end_task();
    indicators_while_off_task();
#    ifdef ITON_BT_DEBUG
    debug_task();
#    endif
    housekeeping_task_user();
}

static void set_color_in_range(uint8_t index, uint8_t led_min, uint8_t led_max, uint8_t r, uint8_t g, uint8_t b) {
    if (index != NO_LED && index >= led_min && index < led_max) {
        rgb_matrix_set_color(index, r, g, b);
    }
}

static bool blink(uint16_t period) {
    return (timer_read() / period) % 2 == 0;
}

// The number row keys "1" to "0" sit at matrix row 1, columns 1 to 10.
static uint8_t number_key_led(uint8_t n) {
    return g_led_config.matrix_co[1][1 + n];
}

bool rgb_matrix_indicators_advanced_kb(uint8_t led_min, uint8_t led_max) {
    if (!rgb_matrix_indicators_advanced_user(led_min, led_max)) {
        return false;
    }

    if (!bt_mode) {
        return true;
    }

    uint8_t profile_led = number_key_led(k2_config.profile);
    switch (bt_state) {
        // Blink against black so the key stands out from any animation.
        case BT_PAIRING:
            if (blink(250)) {
                set_color_in_range(profile_led, led_min, led_max, RGB_BLUE);
            } else {
                set_color_in_range(profile_led, led_min, led_max, RGB_OFF);
            }
            break;
        case BT_CONNECTING:
            if (blink(500)) {
                set_color_in_range(profile_led, led_min, led_max, RGB_WHITE);
            } else {
                set_color_in_range(profile_led, led_min, led_max, RGB_OFF);
            }
            break;
        case BT_CONNECTED:
            if (link_outcome_showing()) {
                set_color_in_range(profile_led, led_min, led_max, RGB_GREEN);
            }
            break;
        case BT_DISCONNECTED:
            if (link_outcome_showing()) {
                set_color_in_range(profile_led, led_min, led_max, RGB_RED);
            }
            break;
        case BT_IDLE:
            break;
    }

    if (battery_showing()) {
        uint8_t keys = battery_level == batt_above_70 ? 10 : battery_level == batt_between_30_70 ? 6 : 3;
        for (uint8_t i = 0; i < keys; i++) {
            switch (battery_level) {
                case batt_above_70:
                    set_color_in_range(number_key_led(i), led_min, led_max, RGB_GREEN);
                    break;
                case batt_between_30_70:
                    set_color_in_range(number_key_led(i), led_min, led_max, RGB_YELLOW);
                    break;
                default:
                    set_color_in_range(number_key_led(i), led_min, led_max, RGB_RED);
                    break;
            }
        }
    }

    if (battery_low && blink(1000)) {
        set_color_in_range(g_led_config.matrix_co[0][0], led_min, led_max, RGB_RED);
    }
#    ifdef K2_SLEEP_DIAG
    // Test builds: when sleep is due but blocked, Esc shows why.
    static const uint8_t blocker_color[][3] = {
        [K2_SLEEP_BLOCKED_BT_TX] = {RGB_RED},
        [K2_SLEEP_BLOCKED_BT_RX] = {RGB_YELLOW},
        [K2_SLEEP_BLOCKED_KEY]   = {RGB_MAGENTA},
        [K2_SLEEP_BLOCKED_USB]   = {RGB_BLUE},
    };
    if (sleep_blocker != K2_SLEEP_OK) {
        const uint8_t *c = blocker_color[sleep_blocker];
        set_color_in_range(g_led_config.matrix_co[0][0], led_min, led_max, c[0], c[1], c[2]);
    }
#    endif
    return true;
}
#endif
