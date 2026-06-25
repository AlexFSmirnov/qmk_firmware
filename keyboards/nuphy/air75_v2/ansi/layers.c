/*
 * Per-layer LED overlay system.
 *
 * On any non-base layer:
 *   1. Every key that has a non-transparent assignment is lit white at the
 *      user's current RGB brightness (read through `keymap_key_to_keycode`
 *      so VIA edits are honoured automatically).
 *   2. Per-key overrides defined below replace that default white.
 *   3. If a side color is configured, both side strips are painted with it
 *      (scaled by user brightness) and the underlying side animation is
 *      suppressed.
 *
 * Base layer = whichever layer `default_layer_state` points to (Mac=0,
 * Win=2). No overlay runs on the base layer.
 *
 * Macro layer (6) intentionally has no entry here; macro_render_indicators
 * in macros.c paints it instead.
 *
 * Edit the `USER-EDITABLE SECTION` below to tune colors.
 */

#include "layers.h"
#include "utils.h"
#include "macros.h"
#include <string.h>

#define SIDE_BOTH(R, G, B)                                                                 \
    .has_side_color = true,                                                                \
    .side_l_r = (R), .side_l_g = (G), .side_l_b = (B),                                     \
    .side_r_r = (R), .side_r_g = (G), .side_r_b = (B)

#define SIDE_SPLIT(LR, LG, LB, RR, RG, RB)                                                 \
    .has_side_color = true,                                                                \
    .side_l_r = (LR), .side_l_g = (LG), .side_l_b = (LB),                                  \
    .side_r_r = (RR), .side_r_g = (RG), .side_r_b = (RB)

#define KEY_COLORS(arr)                                                                    \
    .key_colors = (arr), .key_color_count = sizeof(arr) / sizeof((arr)[0])

/* =========================================================================
 *                          USER-EDITABLE SECTION
 * ========================================================================= */

/* ---- Fn layers (Mac Fn = 1, Win Fn = 3) ----
 * Top row: empty.
 * Number row: 1..3 blue, 4 yellow.
 * QWERTY row: Q..R red, T..I green, O..] blue (F13..F24 grouped 4/4/4).
 * Plus BAT_NUM at row 4 col 6 (the B key) - dim white.
 */
#define BLUE_R    0x00
#define BLUE_G    0x20
#define BLUE_B    0xFF
#define YELLOW_R  0xFF
#define YELLOW_G  0xC0
#define YELLOW_B  0x00
#define F_RED_R   0xFF
#define F_RED_G   0x00
#define F_RED_B   0x00
#define F_GRN_R   0x00
#define F_GRN_G   0xFF
#define F_GRN_B   0x00
#define F_BLU_R   0x00
#define F_BLU_G   0x40
#define F_BLU_B   0xFF
#define BAT_NUM_R 0x60
#define BAT_NUM_G 0x60
#define BAT_NUM_B 0x60

static const key_color_t PROGMEM fn_layer_keys[] = {
    /* Number row 1..4 */
    {1, 1, BLUE_R, BLUE_G, BLUE_B},
    {1, 2, BLUE_R, BLUE_G, BLUE_B},
    {1, 3, BLUE_R, BLUE_G, BLUE_B},
    {1, 4, YELLOW_R, YELLOW_G, YELLOW_B},
    /* QWERTY row Q..R - F13..F16 red */
    {2, 1, F_RED_R, F_RED_G, F_RED_B},
    {2, 2, F_RED_R, F_RED_G, F_RED_B},
    {2, 3, F_RED_R, F_RED_G, F_RED_B},
    {2, 4, F_RED_R, F_RED_G, F_RED_B},
    /* QWERTY row T..I - F17..F20 green */
    {2, 5, F_GRN_R, F_GRN_G, F_GRN_B},
    {2, 6, F_GRN_R, F_GRN_G, F_GRN_B},
    {2, 7, F_GRN_R, F_GRN_G, F_GRN_B},
    {2, 8, F_GRN_R, F_GRN_G, F_GRN_B},
    /* QWERTY row O..] - F21..F24 blue */
    {2, 9,  F_BLU_R, F_BLU_G, F_BLU_B},
    {2, 10, F_BLU_R, F_BLU_G, F_BLU_B},
    {2, 11, F_BLU_R, F_BLU_G, F_BLU_B},
    {2, 12, F_BLU_R, F_BLU_G, F_BLU_B},
    /* BAT_NUM key (B = matrix [4][6]) - dim white */
    {4, 6, BAT_NUM_R, BAT_NUM_G, BAT_NUM_B},
};

/* ---- Vim navigation layer (4) ----
 * Highlight the hjkl arrow cluster.
 */
#define VIM_NAV_R 0x00
#define VIM_NAV_G 0x80
#define VIM_NAV_B 0xFF
static const key_color_t PROGMEM vim_nav_layer_keys[] = {
    {3, 6, VIM_NAV_R, VIM_NAV_G, VIM_NAV_B}, /* h */
    {3, 7, VIM_NAV_R, VIM_NAV_G, VIM_NAV_B}, /* j */
    {3, 8, VIM_NAV_R, VIM_NAV_G, VIM_NAV_B}, /* k */
    {3, 9, VIM_NAV_R, VIM_NAV_G, VIM_NAV_B}, /* l */
};

/* ---- Config layer (5) - RGB matrix + side LED + system ----
 * RGB matrix controls on the left half (Q-T / A-G), side LED controls on
 * the right half (U-P / J-;), system functions on the bottom (B/N/M).
 */
#define RGB_F_R 0x40        /* RGB forward (next) - bright cyan */
#define RGB_F_G 0xC0
#define RGB_F_B 0xFF
#define RGB_B_R 0x10        /* RGB backward (prev) - dim cyan */
#define RGB_B_G 0x40
#define RGB_B_B 0x60
#define SIDE_F_R 0xFF       /* side forward - bright magenta */
#define SIDE_F_G 0x40
#define SIDE_F_B 0xC0
#define SIDE_B_R 0x60       /* side backward - dim magenta */
#define SIDE_B_G 0x10
#define SIDE_B_B 0x40

static const key_color_t PROGMEM config_layer_keys[] = {
    /* Row 2 (Q..) - forward direction */
    {2, 1, RGB_F_R, RGB_F_G, RGB_F_B},    /* Q: RGB_MOD */
    {2, 2, RGB_F_R, RGB_F_G, RGB_F_B},    /* W: RGB_VAI */
    {2, 3, RGB_F_R, RGB_F_G, RGB_F_B},    /* E: RGB_HUI */
    {2, 4, RGB_F_R, RGB_F_G, RGB_F_B},    /* R: RGB_SPI */
    {2, 7, SIDE_F_R, SIDE_F_G, SIDE_F_B}, /* U: SIDE_MOD */
    {2, 8, SIDE_F_R, SIDE_F_G, SIDE_F_B}, /* I: SIDE_VAI */
    {2, 9, SIDE_F_R, SIDE_F_G, SIDE_F_B}, /* O: SIDE_HUI */
    {2, 10, SIDE_F_R, SIDE_F_G, SIDE_F_B},/* P: SIDE_SPI */
    /* Row 3 (A..) - backward direction */
    {3, 1, RGB_B_R, RGB_B_G, RGB_B_B},    /* A: RGB_RMOD */
    {3, 2, RGB_B_R, RGB_B_G, RGB_B_B},    /* S: RGB_VAD */
    {3, 3, RGB_B_R, RGB_B_G, RGB_B_B},    /* D: RGB_HUD */
    {3, 4, RGB_B_R, RGB_B_G, RGB_B_B},    /* F: RGB_SPD */
    {3, 7, SIDE_B_R, SIDE_B_G, SIDE_B_B}, /* J: SIDE_RMOD */
    {3, 8, SIDE_B_R, SIDE_B_G, SIDE_B_B}, /* K: SIDE_VAD */
    {3, 9, SIDE_B_R, SIDE_B_G, SIDE_B_B}, /* L: SIDE_HUD */
    {3, 10, SIDE_B_R, SIDE_B_G, SIDE_B_B},/* ;: SIDE_SPD */
    /* System row (Shift row) - matrix col numbers (V=5, B=6, N=7, M=8) */
    {4, 6, 0x00, 0xFF, 0x00}, /* B: BAT_SHOW   - green */
    {4, 7, 0xFF, 0x80, 0x00}, /* N: SLEEP_MODE - orange */
    {4, 8, 0xFF, 0x00, 0x00}, /* M: DEV_RESET  - red */
};

/* Layer overlays. Layers not listed have no overlay; layers 0 / 2 are
 * intentionally absent (base layers).
 * Macro layer (6) has only a side color; keys are painted by macros.c. */
static const key_color_t PROGMEM empty_keys[1] = {{0}};
const layer_overlay_t layer_overlays[] = {
    [MAC_FN_LAYER]   = { SIDE_BOTH(0x20, 0x40, 0xFF), KEY_COLORS(fn_layer_keys) },
    [WIN_FN_LAYER]   = { SIDE_BOTH(0x20, 0x40, 0xFF), KEY_COLORS(fn_layer_keys) },
    [VIM_NAV_LAYER]  = { SIDE_BOTH(0x00, 0xC0, 0x40), KEY_COLORS(vim_nav_layer_keys) },
    /* Config layer intentionally has NO side color so the stock side LED
     * animation keeps running while you adjust RGB / side settings - lets
     * you see the live preview as you tweak. */
    [CONFIG_LAYER]   = { .has_side_color = false, KEY_COLORS(config_layer_keys) },
    [MACRO_LAYER]    = { SIDE_BOTH(0xFF, 0x80, 0x00), .key_colors = empty_keys, .key_color_count = 0 },
};
const uint8_t layer_overlay_count = sizeof(layer_overlays) / sizeof(layer_overlays[0]);

/* ===================== END USER-EDITABLE SECTION ===================== */


static inline uint8_t current_layer(void) {
    return get_highest_layer(layer_state | default_layer_state);
}

static inline uint8_t base_layer(void) {
    return get_highest_layer(default_layer_state);
}

void layer_overlay_render_keys(void) {
    uint8_t cur  = current_layer();
    uint8_t base = base_layer();
    if (cur == base) return;

    /* Macro layer paints itself. */
    if (cur == MACRO_LAYER) {
        macro_render_indicators();
        return;
    }

    uint8_t v = rgb_matrix_get_val();

    /* Pass 1: paint every key that has a non-transparent mapping. */
    for (uint8_t r = 0; r < MATRIX_ROWS; r++) {
        for (uint8_t c = 0; c < MATRIX_COLS; c++) {
            uint8_t idx = g_led_config.matrix_co[r][c];
            if (idx == NO_LED) continue;

            uint16_t kc = keymap_key_to_keycode(cur, (keypos_t){.row = r, .col = c});
            if (kc == KC_NO || kc == KC_TRNS) continue;

            rgb_matrix_set_color(idx, v, v, v);
        }
    }

    /* Pass 2: per-key overrides. Only applied to keys that actually have a
     * non-transparent assignment on this layer, so a misconfigured (row, col)
     * cannot light up an unrelated key. */
    if (cur >= layer_overlay_count) return;
    const layer_overlay_t *ov = &layer_overlays[cur];
    for (uint16_t i = 0; i < ov->key_color_count; i++) {
        key_color_t k;
        memcpy_P(&k, &ov->key_colors[i], sizeof(k));
        if (k.row >= MATRIX_ROWS || k.col >= MATRIX_COLS) continue;
        uint16_t kc = keymap_key_to_keycode(cur, (keypos_t){.row = k.row, .col = k.col});
        if (kc == KC_NO || kc == KC_TRNS) continue;
        set_key_rgb(k.row, k.col, scale8(k.r, v), scale8(k.g, v), scale8(k.b, v));
    }
}

bool layer_overlay_render_sides(void) {
    uint8_t cur  = current_layer();
    uint8_t base = base_layer();
    if (cur == base) return false;
    if (cur >= layer_overlay_count) return false;

    const layer_overlay_t *ov = &layer_overlays[cur];
    if (!ov->has_side_color) return false;

    /* Scale by the SIDE LED brightness (controlled by SIDE_VAI / SIDE_VAD)
     * rather than the matrix brightness, so the regular side-brightness
     * controls work as expected on overlay colors. */
    set_side_l_rgb_side(ov->side_l_r, ov->side_l_g, ov->side_l_b);
    set_side_r_rgb_side(ov->side_r_r, ov->side_r_g, ov->side_r_b);
    return true;
}
