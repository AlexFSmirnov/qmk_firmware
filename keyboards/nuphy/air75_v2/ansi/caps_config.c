/*
 * Caps Lock / vim-nav role swap for the Caps Lock key.
 *
 * Default: base = MO(VIM_NAV), Fn = Caps Lock.
 * Swapped: base = Caps Lock, Fn = MO(VIM_NAV).
 * Toggle the assignment in the config layer (Fn + Del).
 */

#include "caps_config.h"
#include "ansi.h"
#include "config_ui.h"
#include "layers.h"
#include "utils.h"

extern user_config_t user_config;

#define CAPS_GRN_R 0x20
#define CAPS_GRN_G 0xFF
#define CAPS_GRN_B 0x60
#define CAPS_CY_R  0x00
#define CAPS_CY_G  0x80
#define CAPS_CY_B  0xFF

bool caps_nav_is_default(void) {
    return user_config.caps_nav_default != 0;
}

static bool caps_on_fn_layer(void) {
    return layer_state_is(MAC_FN_LAYER) || layer_state_is(WIN_FN_LAYER);
}

static bool caps_wants_nav(void) {
    if (layer_state_is(CONFIG_LAYER)) {
        return false;
    }
    if (caps_on_fn_layer()) {
        return !caps_nav_is_default();
    }
    return caps_nav_is_default();
}

static void caps_vim_nav_layer(bool pressed) {
    if (pressed) {
        layer_on(VIM_NAV_LAYER);
    } else {
        layer_off(VIM_NAV_LAYER);
    }
}

static void caps_toggle_setting(void) {
    user_config.caps_nav_default = caps_nav_is_default() ? 0 : 1;
    user_config_save();
}

bool caps_config_process_record(uint16_t keycode, keyrecord_t *record) {
    if (keycode != CAPS_ROLE) {
        return true;
    }

    if (layer_state_is(CONFIG_LAYER)) {
        if (record->event.pressed) {
            caps_toggle_setting();
        }
        return false;
    }

    if (caps_wants_nav()) {
        caps_vim_nav_layer(record->event.pressed);
        return false;
    }

    if (record->event.pressed) {
        tap_code(KC_CAPS);
    }
    return false;
}

static void paint_caps_key(uint8_t r, uint8_t g, uint8_t b, uint8_t v) {
    set_key_rgb(CAPS_KEY_ROW, CAPS_KEY_COL, scale8(r, v), scale8(g, v), scale8(b, v));
}

void caps_config_render_indicators(void) {
    uint8_t cur  = get_highest_layer(layer_state | default_layer_state);
    uint8_t base = get_highest_layer(default_layer_state);
    uint8_t v    = rgb_matrix_get_val();
    bool    caps = host_keyboard_led_state().caps_lock;

    if (cur == CONFIG_LAYER) {
        if (caps_nav_is_default()) {
            paint_caps_key(CAPS_CY_R, CAPS_CY_G, CAPS_CY_B, v);
        } else {
            paint_caps_key(CAPS_GRN_R, CAPS_GRN_G, CAPS_GRN_B, v);
        }
        return;
    }

    if (cur == MAC_FN_LAYER || cur == WIN_FN_LAYER) {
        if (caps) {
            paint_caps_key(CAPS_GRN_R, CAPS_GRN_G, CAPS_GRN_B, v);
        } else {
            paint_caps_key(v, v, v, 0xFF);
        }
        return;
    }

    if (cur == base && caps) {
        paint_caps_key(CAPS_GRN_R, CAPS_GRN_G, CAPS_GRN_B, v);
    }
}
