/*
 * NuPhy Air75 V2 ANSI - custom keymap.
 *
 * Layer map (see layers.h / macros.h for the named constants and the short
 * L_MAC / L_MFN / L_WIN / L_WFN / L_VIM / L_CFG / L_MCR aliases used in
 * MO() calls below):
 *
 *   0  Mac base
 *   1  Mac Fn          (held via Fn key)
 *   2  Win base
 *   3  Win Fn          (held via Fn key)
 *   4  Vim navigation  (held via Caps Lock on both Mac/Win, or Right Alt)
 *   5  Config          (held via Fn + Del) - RGB matrix, side LED, system
 *   6  Macros          (held via Right Ctrl position) - 6 macro slots (F7-F12)
 *
 * `_______` (= KC_TRNS) is used everywhere on the overlay layers for keys
 * that aren't explicitly remapped. KC_TRNS falls through to the layer
 * below, which means modifiers (Shift/Ctrl/Cmd/...) and base-layer keys
 * still work while an overlay is held. The active-key whitening in
 * layers.c lights only keys that have a *non-transparent* assignment on
 * the active overlay, so falling through never adds stray highlights.
 *
 * Use `XXXXXXX` (= KC_NO) instead of `_______` only if you want a
 * position to actively swallow the keypress (no fallthrough). Currently
 * unused in this keymap.
 *
 * Notable bindings (apply to both Mac and Win):
 *   - Caps Lock        -> MO(L_VIM)   (hjkl arrows held layer)
 *   - Right Alt        -> MO(L_VIM)   (same, for right hand)
 *   - Right Ctrl       -> MO(L_MCR)   (macro layer)
 *   - Right column     -> VIM_TOGGLE / F13 / F14 / F15 (was PgUp/PgDn/Home/End;
 *                         vim mode keeps the toggle within reach of the home row).
 *   - Fn + PgUp pos    -> VIM_LOCK    (hard-disables vim, red indicator)
 *   - Fn + Del         -> MO(L_CFG)   (RGB / side / system config)
 *   - Fn + 1..3        -> LNK_BLE1..3
 *   - Fn + 4           -> LNK_RF
 *   - Fn + Q..]        -> F13..F24
 *   - Fn + B           -> BAT_NUM     (battery percentage display)
 *
 * Column alignment: every cell below is exactly 12 characters wide so the
 * source visually mirrors the physical key grid. Wide keys (BSpc, BSls,
 * Enter, LShift, RShift, Space) occupy multiple cells; an "empty cell" is
 * a 12-char run of spaces. Please keep this convention if you edit.
 */

#include QMK_KEYBOARD_H
#include "layers.h"
#include "macros.h"

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

    /* ====================================================================
     * Layer 0 - Mac base
     * ==================================================================== */
    [MAC_BASE_LAYER] = LAYOUT_ansi_84(
        KC_ESC,     KC_F1,      KC_F2,      KC_F3,      KC_F4,      KC_F5,      KC_F6,      KC_F7,      KC_F8,      KC_F9,      KC_F10,     KC_F11,     KC_F12,     KC_HOME,    KC_END,     KC_DEL,
        KC_GRV,     KC_1,       KC_2,       KC_3,       KC_4,       KC_5,       KC_6,       KC_7,       KC_8,       KC_9,       KC_0,       KC_MINS,    KC_EQL,     KC_BSPC,                VIM_TOGGLE,
        KC_TAB,     KC_Q,       KC_W,       KC_E,       KC_R,       KC_T,       KC_Y,       KC_U,       KC_I,       KC_O,       KC_P,       KC_LBRC,    KC_RBRC,    KC_BSLS,                KC_F13,
        MO(L_VIM),  KC_A,       KC_S,       KC_D,       KC_F,       KC_G,       KC_H,       KC_J,       KC_K,       KC_L,       KC_SCLN,    KC_QUOT,    KC_ENT,                             KC_F14,
        KC_LSFT,                KC_Z,       KC_X,       KC_C,       KC_V,       KC_B,       KC_N,       KC_M,       KC_COMM,    KC_DOT,     KC_SLSH,    KC_RSFT,                KC_UP,      KC_F15,
        KC_LCTL,    KC_LALT,    KC_LGUI,    KC_SPC,                                                                              MO(L_VIM),  MO(L_MFN),  MO(L_MCR),  KC_LEFT,    KC_DOWN,    KC_RGHT
    ),

    /* ====================================================================
     * Layer 1 - Mac Fn
     *
     * Mac base now has plain F1..F12 on the F-row (same as Win base).
     * Holding Fn turns those positions into Mac media / brightness / system
     * controls (BRID, BRIU, MAC_TASK, MAC_SEARCH, MAC_VOICE, MAC_DND,
     * MPRV, MPLY, MNXT, MUTE, VOLD, VOLU) - the original NuPhy F-row.
     *
     *   Fn + F1   : KC_BRID
     *   Fn + F2   : KC_BRIU
     *   Fn + F3   : MAC_TASK
     *   Fn + F4   : MAC_SEARCH
     *   Fn + F5   : MAC_VOICE
     *   Fn + F6   : MAC_DND
     *   Fn + F7   : KC_MPRV
     *   Fn + F8   : KC_MPLY
     *   Fn + F9   : KC_MNXT
     *   Fn + F10  : KC_MUTE
     *   Fn + F11  : KC_VOLD
     *   Fn + F12  : KC_VOLU
     *   Fn + Del  : MO(L_CFG)
     *   Fn + PgUp : VIM_LOCK
     *   Fn + 1..4 : LNK_BLE1..3 + LNK_RF
     *   Fn + Q..] : F13..F24
     *   Fn + B    : BAT_NUM
     * Everything else falls through (so Fn + Shift etc. still work).
     * ==================================================================== */
    [MAC_FN_LAYER] = LAYOUT_ansi_84(
        _______,    KC_BRID,    KC_BRIU,    MAC_TASK,   MAC_SEARCH, MAC_VOICE,  MAC_DND,    KC_MPRV,    KC_MPLY,    KC_MNXT,    KC_MUTE,    KC_VOLD,    KC_VOLU,    _______,    _______,    MO(L_CFG),
        _______,    LNK_BLE1,   LNK_BLE2,   LNK_BLE3,   LNK_RF,     _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                VIM_LOCK,
        _______,    KC_F13,     KC_F14,     KC_F15,     KC_F16,     KC_F17,     KC_F18,     KC_F19,     KC_F20,     KC_F21,     KC_F22,     KC_F23,     KC_F24,     _______,                _______,
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                            _______,
        _______,                _______,    _______,    _______,    _______,    BAT_NUM,    _______,    _______,    _______,    _______,    _______,    _______,                _______,    _______,
        _______,    _______,    _______,    _______,                                                                             _______,    _______,    _______,    _______,    _______,    _______
    ),

    /* ====================================================================
     * Layer 2 - Win base
     * ==================================================================== */
    [WIN_BASE_LAYER] = LAYOUT_ansi_84(
        KC_ESC,     KC_F1,      KC_F2,      KC_F3,      KC_F4,      KC_F5,      KC_F6,      KC_F7,      KC_F8,      KC_F9,      KC_F10,     KC_F11,     KC_F12,     KC_HOME,    KC_END,     KC_DEL,
        KC_GRV,     KC_1,       KC_2,       KC_3,       KC_4,       KC_5,       KC_6,       KC_7,       KC_8,       KC_9,       KC_0,       KC_MINS,    KC_EQL,     KC_BSPC,                VIM_TOGGLE,
        KC_TAB,     KC_Q,       KC_W,       KC_E,       KC_R,       KC_T,       KC_Y,       KC_U,       KC_I,       KC_O,       KC_P,       KC_LBRC,    KC_RBRC,    KC_BSLS,                KC_F13,
        MO(L_VIM),  KC_A,       KC_S,       KC_D,       KC_F,       KC_G,       KC_H,       KC_J,       KC_K,       KC_L,       KC_SCLN,    KC_QUOT,    KC_ENT,                             KC_F14,
        KC_LSFT,                KC_Z,       KC_X,       KC_C,       KC_V,       KC_B,       KC_N,       KC_M,       KC_COMM,    KC_DOT,     KC_SLSH,    KC_RSFT,                KC_UP,      KC_F15,
        KC_LCTL,    KC_LGUI,    KC_LALT,    KC_SPC,                                                                              MO(L_VIM),  MO(L_WFN),  MO(L_MCR),  KC_LEFT,    KC_DOWN,    KC_RGHT
    ),

    /* ====================================================================
     * Layer 3 - Win Fn
     *
     * Win base has actual F1..F12 on the F-row. Holding Fn swaps those
     * positions to media / brightness controls (mirror of how Mac base
     * shows media and Mac Fn shows F-keys).
     *
     *   Fn + F1   : KC_BRID   (brightness down)
     *   Fn + F2   : KC_BRIU   (brightness up)
     *   Fn + F7   : KC_MPRV
     *   Fn + F8   : KC_MPLY
     *   Fn + F9   : KC_MNXT
     *   Fn + F10  : KC_MUTE
     *   Fn + F11  : KC_VOLD
     *   Fn + F12  : KC_VOLU
     *   Fn + Del  : MO(L_CFG)
     *   Fn + PgUp : VIM_LOCK
     *   Fn + 1..4 : LNK_BLE1..3 + LNK_RF
     *   Fn + Q..] : F13..F24
     *   Fn + B    : BAT_NUM
     * F3..F6 left transparent (no good cross-platform equivalent of the
     * Mac-specific MAC_TASK / SEARCH / VOICE / DND keys).
     * ==================================================================== */
    [WIN_FN_LAYER] = LAYOUT_ansi_84(
        _______,    KC_BRID,    KC_BRIU,    _______,    _______,    _______,    _______,    KC_MPRV,    KC_MPLY,    KC_MNXT,    KC_MUTE,    KC_VOLD,    KC_VOLU,    _______,    _______,    MO(L_CFG),
        _______,    LNK_BLE1,   LNK_BLE2,   LNK_BLE3,   LNK_RF,     _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                VIM_LOCK,
        _______,    KC_F13,     KC_F14,     KC_F15,     KC_F16,     KC_F17,     KC_F18,     KC_F19,     KC_F20,     KC_F21,     KC_F22,     KC_F23,     KC_F24,     _______,                _______,
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                            _______,
        _______,                _______,    _______,    _______,    _______,    BAT_NUM,    _______,    _______,    _______,    _______,    _______,    _______,                _______,    _______,
        _______,    _______,    _______,    _______,                                                                             _______,    _______,    _______,    _______,    _______,    _______
    ),

    /* ====================================================================
     * Layer 4 - Vim navigation (hjkl -> arrows; Left Alt+H/L -> Home/End)
     * ==================================================================== */
    [VIM_NAV_LAYER] = LAYOUT_ansi_84(
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                _______,
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                _______,
        _______,    _______,    _______,    _______,    _______,    _______,    KC_LEFT,    KC_DOWN,    KC_UP,      KC_RGHT,    _______,    _______,    _______,                            _______,
        _______,                _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                _______,    _______,
        _______,    _______,    _______,    _______,                                                                             _______,    _______,    _______,    _______,    _______,    _______
    ),

    /* ====================================================================
     * Layer 5 - Config (RGB matrix + side LED + system)
     *
     *   Q/A: RGB_MOD  / RGB_RMOD     (effect next / prev)
     *   W/S: RGB_VAI  / RGB_VAD      (matrix brightness)
     *   E/D: RGB_HUI  / RGB_HUD      (matrix hue)
     *   R/F: RGB_SPI  / RGB_SPD      (matrix speed)
     *   U/J: SIDE_MOD / SIDE_RMOD    (side mode next / prev)
     *   I/K: SIDE_VAI / SIDE_VAD     (side brightness)
     *   O/L: SIDE_HUI / SIDE_HUD     (side color next / prev)
     *   P/;: SIDE_SPI / SIDE_SPD     (side speed)
     *   B  : BAT_SHOW
     *   N  : SLEEP_MODE
     *   M  : DEV_RESET (factory reset, long press)
     * ==================================================================== */
    [CONFIG_LAYER] = LAYOUT_ansi_84(
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                _______,
        _______,    RGB_MOD,    RGB_VAI,    RGB_HUI,    RGB_SPI,    _______,    _______,    SIDE_MOD,   SIDE_VAI,   SIDE_HUI,   SIDE_SPI,   _______,    _______,    _______,                _______,
        _______,    RGB_RMOD,   RGB_VAD,    RGB_HUD,    RGB_SPD,    _______,    _______,    SIDE_RMOD,  SIDE_VAD,   SIDE_HUD,   SIDE_SPD,   _______,    _______,                            _______,
        _______,                _______,    _______,    _______,    _______,    BAT_SHOW,   SLEEP_MODE, DEV_RESET,  _______,    _______,    _______,    _______,                _______,    _______,
        _______,    _______,    _______,    _______,                                                                             _______,    _______,    _______,    _______,    _______,    _______
    ),

    /* ====================================================================
     * Layer 6 - Macros
     *
     * All key positions are `_______`; the actual behaviour is implemented
     * in macros.c by detecting (row, col) directly in process_record_user.
     * The macro layer consumes every keypress while held, so KC_TRNS vs
     * KC_NO is functionally irrelevant here - we use `_______` for visual
     * consistency with the other overlays.
     *
     *   F7..F12 : erase occupied slot (red)
     *   7..=    : record/save (empty, white) or play with delays (yellow)
     *   U..]    : play instantly (green)
     *   Esc     : cancel an in-progress recording
     * ==================================================================== */
    [MACRO_LAYER] = LAYOUT_ansi_84(
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                _______,
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                _______,
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                            _______,
        _______,                _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                _______,    _______,
        _______,    _______,    _______,    _______,                                                                             _______,    _______,    _______,    _______,    _______,    _______
    )
};
