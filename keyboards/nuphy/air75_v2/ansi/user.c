#include "user.h"
#include "ansi.h"
#include "timer.h"
#include "utils.h"
#include "layers.h"
#include "macros.h"
#include "qmk-vim/src/vim.h"
#include "qmk-vim/src/modes.h"

bool     vim_locked_disabled = false;
uint16_t vim_j_last_pressed  = 0;

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    /* Macro layer / recording capture has top priority. If it consumed the
     * event we stop processing. */
    if (macro_process_record(keycode, record)) {
        return false;
    }

    if (!process_vim_mode(keycode, record)) {
        return false;
    }

    switch (keycode) {
        case VIM_LOCK:
            if (record->event.pressed) {
                vim_locked_disabled = !vim_locked_disabled;
                disable_vim_mode();
            }
            return false;

        case VIM_TOGGLE:
            if (vim_locked_disabled) {
                return false;
            }
            if (record->event.pressed) {
                toggle_vim_mode();
            }
            return false;

        case VIM_HOLD:
            if (vim_locked_disabled) {
                return false;
            }
            if (record->event.pressed) {
                enable_vim_mode();
            } else {
                disable_vim_mode();
            }
            break;

        case KC_H:
            if (record->event.pressed && (get_mods() & MOD_MASK_CTRL) && (get_mods() & MOD_MASK_GUI)) {
                tap_code16(LCTL(LGUI(KC_LEFT)));
                return false;
            }
            break;

        case KC_L:
            if (record->event.pressed && (get_mods() & MOD_MASK_CTRL) && (get_mods() & MOD_MASK_GUI)) {
                tap_code16(LCTL(LGUI(KC_RIGHT)));
                return false;
            }
            break;

        default:
            return true;
    }

    return true;
}

bool process_insert_mode_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        if (keycode == KC_J) {
            uint16_t now = timer_read();
            if (now - vim_j_last_pressed < VIM_DOUBLE_J_DELAY) {
                tap_code(KC_BSPC);
                normal_mode();
                vim_j_last_pressed = 0;
                return false;
            }
            vim_j_last_pressed = now;
        } else {
            vim_j_last_pressed = 0;
        }

        if (keycode == LCTL(KC_W)) {
            tap_code16(LCTL(KC_BSPC));
            return false;
        }
    }

    return true;
}

void housekeeping_task_user(void) {
    macro_task();
}

static void rgb_matrix_vim_mode(void) {
    uint8_t brightness = scale8(rgb_matrix_get_val(), 200);

    if (vim_locked_disabled) {
        set_key_rgb(1, 16, brightness, 0x00, 0x00);
        return;
    }
    if (vim_mode_enabled()) {
        set_key_rgb(1, 16, 0x00, brightness, 0x00);
    }
}

bool rgb_matrix_indicators_user(void) {
    layer_overlay_render_keys();
    rgb_matrix_vim_mode();
    return true;
}

static void side_led_vim_mode(void) {
    HSV hsv = rgb_matrix_get_hsv();
    switch (get_vim_mode()) {
        case NORMAL_MODE:
            hsv.h = 140;
            break;
        case INSERT_MODE:
            hsv.h = 90;
            break;
        case VISUAL_MODE:
        case VISUAL_LINE_MODE:
            hsv.h = 35;
            break;
        default:
            break;
    }
    set_sides_hsv_full(hsv.h, hsv.s, hsv.v);
}

bool side_led_show_user(void) {
    /* Priority order: macro recording/playback > vim > layer overlay > stock animation */
    if (macro_render_sides()) {
        return false;
    }
    if (vim_mode_enabled()) {
        side_led_vim_mode();
        return false;
    }
    if (layer_overlay_render_sides()) {
        return false;
    }
    return true;
}
