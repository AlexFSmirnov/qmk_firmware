#include "ansi.h"

bool test_key_color = 0;
uint16_t test_index = 0;

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        /* case KC_8: */
        /*     if (record->event.pressed) { */
        /*         test_key_color = !test_key_color; */
        /*     } */
        /*     return true; */
        default:
            return true;
    }
}

uint8_t get_key_rgb_index(uint8_t row, uint8_t col) {
    // TODO: Doesn't really work, it goes like a snake
    return row * 17 + col;
}

bool rgb_matrix_indicators_user(void)
{
    if (test_key_color) {
        if (test_index >= RGB_MATRIX_LED_COUNT) {
            test_index = 0;
        }
        rgb_matrix_set_color(test_index, 0x00, 0xFF, 0x00);
        test_index++;
        wait_ms(100);
        return false;
    }

    return true;
}
