#include "utils.h"
#include "quantum.h"
#include "eeconfig.h"
#include "ansi.h"

void set_left_rgb(uint8_t r, uint8_t g, uint8_t b);
void set_right_rgb(uint8_t r, uint8_t g, uint8_t b);
void side_rgb_set_color(int index, uint8_t red, uint8_t green, uint8_t blue);

extern user_config_t user_config;

uint8_t scale8(uint8_t value, uint8_t scale) {
    return ((uint16_t)value * (uint16_t)scale) >> 8;
}

void set_key_rgb(uint8_t row, uint8_t col, uint8_t r, uint8_t g, uint8_t b) {
    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) return;
    uint8_t idx = g_led_config.matrix_co[row][col];
    if (idx == NO_LED) return;
    rgb_matrix_set_color(idx, r, g, b);
}

void set_key_hsv(uint8_t row, uint8_t col, uint8_t h, uint8_t s, uint8_t v) {
    RGB rgb = hsv_to_rgb((HSV){h, s, v});
    set_key_rgb(row, col, rgb.r, rgb.g, rgb.b);
}

void set_side_l_rgb(uint8_t r, uint8_t g, uint8_t b) {
    set_left_rgb(r, g, b);
}

void set_side_r_rgb(uint8_t r, uint8_t g, uint8_t b) {
    set_right_rgb(r, g, b);
}

void set_sides_rgb(uint8_t r, uint8_t g, uint8_t b) {
    set_left_rgb(r, g, b);
    set_right_rgb(r, g, b);
}

void set_sides_hsv(uint8_t h, uint8_t s, uint8_t v) {
    RGB rgb = hsv_to_rgb((HSV){h, s, v});
    set_sides_rgb(rgb.r, rgb.g, rgb.b);
}

void set_side_l_rgb_full(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < 6; i++) side_rgb_set_color(i, r, g, b);
}

void set_side_r_rgb_full(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 6; i < 12; i++) side_rgb_set_color(i, r, g, b);
}

void set_sides_rgb_full(uint8_t r, uint8_t g, uint8_t b) {
    set_side_l_rgb_full(r, g, b);
    set_side_r_rgb_full(r, g, b);
}

void set_sides_hsv_full(uint8_t h, uint8_t s, uint8_t v) {
    RGB rgb = hsv_to_rgb((HSV){h, s, v});
    set_sides_rgb_full(rgb.r, rgb.g, rgb.b);
}

/* ---------- side-brightness aware painters ---------- */

extern uint8_t       side_light;
extern const uint8_t side_light_table[11];

uint8_t side_brightness_scale(void) {
    uint8_t level = side_light;
    if (level > 10) level = 10;
    return side_light_table[level];
}

void set_side_l_rgb_side(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t s = side_brightness_scale();
    set_side_l_rgb_full(scale8(r, s), scale8(g, s), scale8(b, s));
}

void set_side_r_rgb_side(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t s = side_brightness_scale();
    set_side_r_rgb_full(scale8(r, s), scale8(g, s), scale8(b, s));
}

void set_sides_rgb_side(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t s = side_brightness_scale();
    set_sides_rgb_full(scale8(r, s), scale8(g, s), scale8(b, s));
}

void set_sides_hsv_side(uint8_t h, uint8_t s, uint8_t v) {
    RGB rgb = hsv_to_rgb((HSV){h, s, v});
    set_sides_rgb_side(rgb.r, rgb.g, rgb.b);
}

/* ---------- user_config persistence ---------- */

void user_config_load(void) {
    if (eeconfig_is_user_datablock_valid()) {
        eeprom_read_block(&user_config, EECONFIG_USER_DATABLOCK, sizeof(user_config));
    } else {
        memset(&user_config, 0, sizeof(user_config));
    }
}

void user_config_save(void) {
    eeprom_update_block(&user_config, EECONFIG_USER_DATABLOCK, sizeof(user_config));
    eeprom_update_dword(EECONFIG_USER, (EECONFIG_USER_DATA_VERSION));
}
