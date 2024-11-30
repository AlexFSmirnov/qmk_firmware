#include "user.h"
#include "ansi.h"
#include "qmk-vim/src/vim.h"
#include "qmk-vim/src/modes.h"

bool test_key_color = 0;
uint16_t test_index = 0;
uint16_t test_row = 0;
uint16_t test_col = 0;

bool vim_locked_disabled = false;

void set_left_rgb(uint8_t r, uint8_t g, uint8_t b);
void set_right_rgb(uint8_t r, uint8_t g, uint8_t b);
uint8_t scale8(uint8_t value, uint8_t scale);

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (!process_vim_mode(keycode, record)) {
        return false;
    }

    test_row = record->event.key.row;
    test_col = record->event.key.col;
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

            return false;

        default:
            return true;
    }
}

bool process_normal_mode_user(uint16_t keycode, keyrecord_t *record) {
    /* switch (keycode) { */
    /*     case LSFT(KC_H): */
    /*         if (record->event.pressed) { */
    /*             process_record_user(LSFT(KC_6), record); */
    /*         } */
    /*         return false; */
    /*     case LSFT(KC_L): */
    /*         if (record->event.pressed) { */
    /*             process_record_user(LSFT(KC_4), record); */
    /*         } */
    /*         return false; */
    /* } */
    return true;
}

void rgb_matrix_vim_mode(void) {
    uint8_t led_index = g_led_config.matrix_co[1][16];
    uint8_t brightness = scale8(rgb_matrix_config.hsv.v, 200);

    if (vim_locked_disabled) {
        rgb_matrix_set_color(led_index, brightness, 0x00, 0x00);
        return;
    }
    if (vim_mode_enabled()) {
        rgb_matrix_set_color(led_index, 0x00, brightness, 0x00);
        return;
    }
}

bool rgb_matrix_indicators_user(void) {
    rgb_matrix_vim_mode();

    /* if (vim_mode_enabled()) { */
    /*     rgb_matrix_set_color(g_led_config.matrix_co[0][1], 0x00, 0xFF, 0x00); */
    /* } else { */
    /*     rgb_matrix_set_color(g_led_config.matrix_co[0][1], 0xFF, 0x00, 0x00); */
    /* } */

    /* if (vim_state == ENABLED) { */
    /*     rgb_matrix_set_color(g_led_config.matrix_co[0][1], 0x00, 0xFF, 0x00); */
    /* } */
    /* if (vim_state == DISABLED) { */
    /*     rgb_matrix_set_color(g_led_config.matrix_co[0][1], 0x00, 0x00, 0xFF); */
    /* } */

    if (test_key_color) {
        /* if (test_col >= MATRIX_COLS) { */
        /*     test_col = 0; */
        /*     test_row++; */
        /*     if (test_row >= MATRIX_ROWS) { */
        /*         test_row = 0; */
        /*     } */
        /* } */

        rgb_matrix_set_color_all(0x00, 0x00, 0x00);
        rgb_matrix_set_color(g_led_config.matrix_co[test_row][test_col], 0x00, 0x00, 0xFF);
        /* test_col++; */
        /* wait_ms(100); */
        return false;
    }

        /* if (test_index >= RGB_MATRIX_LED_COUNT) { */
        /*     test_index = 0; */
        /* } */
        /* rgb_matrix_set_color(test_index, 0x00, 0xFF, 0x00); */
        /* test_index++; */
        /* wait_ms(100); */
        /* return false; */

    return true;
}

void side_led_vim_mode(void) {
    HSV hsv = rgb_matrix_config.hsv;
    switch (get_vim_mode()) {
        case NORMAL_MODE:
            hsv.h = 160;
            break;
        case INSERT_MODE:
            hsv.h = 100;
            break;
        case VISUAL_MODE:
        case VISUAL_LINE_MODE:
            hsv.h = 35;
            break;
        default:
            break;
    }

    RGB rgb = hsv_to_rgb(hsv);
    set_left_rgb(rgb.r, rgb.g, rgb.b);
    set_right_rgb(rgb.r, rgb.g, rgb.b);
}

bool side_led_show_user(void) {
    if (vim_mode_enabled()) {
        side_led_vim_mode();
        return false;
    }

    return true;
}
