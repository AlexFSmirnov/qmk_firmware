#include "user.h"
#include "ansi.h"
#include "timer.h"
#include "utils.h"
#include "layers.h"
#include "macros.h"
#include "vim_macros.h"
#include "rgb_matrix.h"
#include "config_ui.h"
#include "qmk-vim/src/vim.h"
#include "qmk-vim/src/modes.h"

bool     vim_locked_disabled = false;
uint16_t vim_j_last_pressed  = 0;

static bool vim_nav_lalt_active = false;

bool vim_nav_lalt_held(void) {
    return vim_nav_lalt_active;
}

static bool is_base_lalt_key(keyrecord_t *record) {
    uint8_t base = get_highest_layer(default_layer_state);
    return keymap_key_to_keycode(base, record->event.key) == KC_LALT;
}

/* Left Alt on the vim nav layer is local-only: it toggles HL Home/End mode
 * and must never reach the host. */
static bool vim_nav_lalt_swallow(uint16_t keycode, keyrecord_t *record) {
    if (!IS_LAYER_ON(VIM_NAV_LAYER)) {
        if (!record->event.pressed) {
            vim_nav_lalt_active = false;
        }
        return false;
    }

    if (keycode != KC_LALT || !is_base_lalt_key(record)) {
        return false;
    }

    vim_nav_lalt_active = record->event.pressed;
    return true;
}

static void vim_nav_strip_host_lalt(void) {
    if (!IS_LAYER_ON(VIM_NAV_LAYER)) {
        return;
    }

    uint8_t mods = get_mods();
    if (mods & MOD_LALT) {
        set_mods(mods & ~MOD_LALT);
    }

    uint8_t oneshot = get_oneshot_mods();
    if (oneshot & MOD_LALT) {
        set_oneshot_mods(oneshot & ~MOD_LALT);
    }
}

/* Left Alt on the hjkl nav layer maps H/L to Home/End. Shift is preserved
 * so Alt+Shift+H behaves like Shift+Home. */
static bool vim_nav_alt_home_end(uint16_t keycode, keyrecord_t *record) {
    if (!IS_LAYER_ON(VIM_NAV_LAYER) || !vim_nav_lalt_active) {
        return false;
    }

    keypos_t key = record->event.key;
    bool     is_h = (key.row == 3 && key.col == 6 && keycode == KC_LEFT);
    bool     is_l = (key.row == 3 && key.col == 9 && keycode == KC_RGHT);
    if (!is_h && !is_l) {
        return false;
    }

    const uint8_t  saved_mods    = get_mods();
    const uint8_t  saved_oneshot = get_oneshot_mods();
    const uint8_t  shift_only    = (saved_mods | saved_oneshot) & MOD_MASK_SHIFT;
    const uint8_t  target        = is_h ? KC_HOME : KC_END;
    const uint16_t out           = shift_only ? (uint16_t)LSFT(target) : target;

    clear_mods();
    clear_oneshot_mods();

    if (record->event.pressed) {
        register_code16(out);
    } else {
        unregister_code16(out);
    }

    set_mods(saved_mods);
    set_oneshot_mods(saved_oneshot);
    return true;
}

static uint16_t rgb_hue_repeat_key   = 0;
static uint16_t rgb_hue_repeat_timer = 0;
static bool     rgb_hue_repeat_first = false;

static void rgb_hue_step(uint16_t keycode) {
    const uint8_t shifted = (get_mods() | get_oneshot_mods()) & MOD_MASK_SHIFT;

    if (keycode == RGB_HUI) {
        if (shifted) {
            rgb_matrix_decrease_hue();
        } else {
            rgb_matrix_increase_hue();
        }
    } else if (keycode == RGB_HUD) {
        if (shifted) {
            rgb_matrix_increase_hue();
        } else {
            rgb_matrix_decrease_hue();
        }
    }
}

/* RGB_HUI / RGB_HUD: one step on press, then repeat while held. */
static bool rgb_hue_repeat_process(uint16_t keycode, keyrecord_t *record) {
    if (keycode != RGB_HUI && keycode != RGB_HUD) {
        return false;
    }

    if (record->event.pressed) {
        rgb_hue_step(keycode);
        rgb_hue_repeat_key   = keycode;
        rgb_hue_repeat_timer = timer_read();
        rgb_hue_repeat_first = true;
    } else if (rgb_hue_repeat_key == keycode) {
        rgb_hue_repeat_key = 0;
    }

    return true;
}

static void rgb_hue_repeat_task(void) {
    if (!rgb_hue_repeat_key) {
        return;
    }

    const uint16_t threshold = rgb_hue_repeat_first ? RGB_HUE_REPEAT_DELAY_MS : RGB_HUE_REPEAT_INTERVAL_MS;
    if (timer_elapsed(rgb_hue_repeat_timer) < threshold) {
        return;
    }

    rgb_hue_step(rgb_hue_repeat_key);
    rgb_hue_repeat_timer = timer_read();
    rgb_hue_repeat_first = false;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    /* Macro layer / recording capture has top priority. If it consumed the
     * event we stop processing. */
    if (macro_process_record(keycode, record)) {
        return false;
    }

    if (vim_mode_enabled() && vim_macro_is_playing() && !vim_macro_is_injecting()) {
        if (record->event.pressed && keycode == KC_ESC) {
            vim_macro_stop_playback();
        }
        return false;
    }

    if (rgb_hue_repeat_process(keycode, record)) {
        return false;
    }

    if (!config_process_record(keycode, record)) {
        return false;
    }

    if (vim_nav_lalt_swallow(keycode, record)) {
        return false;
    }

    vim_nav_strip_host_lalt();

    if (vim_nav_alt_home_end(keycode, record)) {
        return false;
    }

    if (vim_mode_enabled()) {
        bool pass = process_vim_mode(keycode, record);
        if (!pass) {
            return false;
        }
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

bool process_normal_mode_user(uint16_t keycode, keyrecord_t *record) {
    return vim_macro_process_normal(keycode, record);
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
    vim_macro_task();
    rgb_hue_repeat_task();
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
    config_render_indicators();
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
    /* Scale by the user's SIDE LED brightness (SIDE_VAI / SIDE_VAD), not
     * the matrix brightness - keeps vim mode in line with the layer
     * overlay and macro recording indicators. */
    set_sides_hsv_side(hsv.h, hsv.s, 0xFF);
}

bool side_led_show_user(void) {
    /* Priority: F7-F12 macro recording > vim macro recording > vim mode > layer overlay */
    if (macro_render_sides()) {
        return false;
    }
    if (vim_macro_render_sides()) {
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
