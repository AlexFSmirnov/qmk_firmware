#pragma once

#include "quantum.h"

uint8_t scale8(uint8_t value, uint8_t scale);

/* Matrix-key painters. (row, col) -> LED, no-op if NO_LED. */
void set_key_rgb(uint8_t row, uint8_t col, uint8_t r, uint8_t g, uint8_t b);
void set_key_hsv(uint8_t row, uint8_t col, uint8_t h, uint8_t s, uint8_t v);

/* Side LED painters - "scaled" variants funnel through the firmware's
 * existing set_left_rgb/set_right_rgb (which divide by 4 for headroom).
 * Use these to match the look of the stock side animations. */
void set_side_l_rgb(uint8_t r, uint8_t g, uint8_t b);
void set_side_r_rgb(uint8_t r, uint8_t g, uint8_t b);
void set_sides_rgb (uint8_t r, uint8_t g, uint8_t b);
void set_sides_hsv (uint8_t h, uint8_t s, uint8_t v);

/* "_full" variants write directly to the side LED buffer with no
 * internal scaling. Use these when you want a real, brightness-respecting
 * effect (e.g. layer overlays scaled by rgb_matrix_get_val()). */
void set_side_l_rgb_full(uint8_t r, uint8_t g, uint8_t b);
void set_side_r_rgb_full(uint8_t r, uint8_t g, uint8_t b);
void set_sides_rgb_full (uint8_t r, uint8_t g, uint8_t b);
void set_sides_hsv_full (uint8_t h, uint8_t s, uint8_t v);

/* "_side" variants scale by the user's SIDE LED brightness (controlled by
 * SIDE_VAI / SIDE_VAD, persisted in `side_light`). Use these for any custom
 * indicator that lives on the side strip - layer overlays, macro recording,
 * vim mode, etc - so the existing brightness controls apply. */
uint8_t side_brightness_scale(void); /* 0..255 (current side_light level) */
void set_side_l_rgb_side(uint8_t r, uint8_t g, uint8_t b);
void set_side_r_rgb_side(uint8_t r, uint8_t g, uint8_t b);
void set_sides_rgb_side (uint8_t r, uint8_t g, uint8_t b);
void set_sides_hsv_side (uint8_t h, uint8_t s, uint8_t v);

/* user_config persistence. The user data block is now sized to also hold
 * macro storage; these helpers do a partial write covering only the
 * user_config bytes (not the whole block). */
void user_config_load(void);
void user_config_save(void);
