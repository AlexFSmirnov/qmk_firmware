#pragma once

#include "quantum.h"

#define MAC_BASE_LAYER    0
#define MAC_FN_LAYER      1
#define WIN_BASE_LAYER    2
#define WIN_FN_LAYER      3
#define VIM_NAV_LAYER     4   /* hjkl arrows */
#define CONFIG_LAYER      5   /* RGB / side LED config + misc system controls */
/* MACRO_LAYER (6) is defined in macros.h */

/* Short aliases for use inside MO()/LT()/TG()/TO() and friends so the
 * keymap's bottom row doesn't blow out to 20+ chars per token. Identical
 * values to the *_LAYER constants above; pick whichever name reads best. */
#define L_MAC  MAC_BASE_LAYER
#define L_MFN  MAC_FN_LAYER
#define L_WIN  WIN_BASE_LAYER
#define L_WFN  WIN_FN_LAYER
#define L_VIM  VIM_NAV_LAYER
#define L_CFG  CONFIG_LAYER
/* L_MCR (macro layer) is defined in macros.h alongside MACRO_LAYER. */

/* Per-key color override stored in PROGMEM. */
typedef struct {
    uint8_t row;
    uint8_t col;
    uint8_t r;
    uint8_t g;
    uint8_t b;
} key_color_t;

/* Optional per-layer overlay describing side LED color and per-key
 * overrides. Side colors are full-range RGB values that will be scaled by
 * the user's RGB brightness via rgb_matrix_get_val(). */
typedef struct {
    bool    has_side_color;
    uint8_t side_l_r, side_l_g, side_l_b;
    uint8_t side_r_r, side_r_g, side_r_b;

    const key_color_t *key_colors;
    uint16_t           key_color_count;
} layer_overlay_t;

extern const layer_overlay_t layer_overlays[];
extern const uint8_t         layer_overlay_count;

/* Paints active-layer keys white (by default) plus any per-key overrides.
 * Call from `rgb_matrix_indicators_user`. */
void layer_overlay_render_keys(void);

/* Paints the configured side color for the active layer (if any).
 * Returns true if it took over the side LEDs (caller should skip its own
 * rendering). Call from `side_led_show_user`. */
bool layer_overlay_render_sides(void);
