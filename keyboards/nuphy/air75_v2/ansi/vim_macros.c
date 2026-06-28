/*
 * Vim-style register macros (q / @).
 *
 * RAM-only; cleared when vim mode is disabled. Recording replays through
 * process_vim_mode so mode changes and LED feedback stay in sync.
 */

#include "vim_macros.h"
#include "utils.h"
#include "macros.h"
#include "rgb_matrix.h"
#include "action.h"
#include "qmk-vim/src/vim.h"
#include "qmk-vim/src/modes.h"
#include "qmk-vim/src/numbered_actions.h"
#include <string.h>

typedef struct {
    uint16_t keycode;
    uint8_t  row;
    uint8_t  col;
} vim_macro_event_t;

#define VIM_MACRO_POOL_SIZE 64

typedef enum {
    VIM_MACRO_IDLE = 0,
    VIM_MACRO_WAIT_REG,
    VIM_MACRO_RECORDING,
    VIM_MACRO_WAIT_PLAY,
} vim_macro_state_t;

static vim_macro_event_t pool[VIM_MACRO_POOL_SIZE];
static uint8_t           pool_used;
static uint8_t           reg_offset[VIM_MACRO_REG_COUNT];
static uint8_t           reg_count[VIM_MACRO_REG_COUNT];

static vim_macro_state_t state          = VIM_MACRO_IDLE;
static uint8_t           active_reg     = 0;
static uint8_t           last_play_reg  = 0;
static bool              skip_capture   = false;
static uint8_t           pending_reg_swallow = 0xFF;
static bool            macro_dot_ready     = false;
static uint8_t         macro_dot_reg       = 0;

static struct {
    bool         active;
    bool         injecting;
    uint8_t      reg;
    int16_t      repeats_left;
    uint8_t      index;
    uint32_t     next_time;
    keyrecord_t  playback_record;
} playback = {0};

extern int16_t motion_counter;

static bool is_at_key(uint16_t keycode) {
    return keycode == LSFT(KC_2);
}

static bool is_reg_letter(uint16_t keycode, uint8_t *reg_out) {
    if (keycode < KC_A || keycode > KC_Z) {
        return false;
    }
    if ((keycode & 0xFF00) != 0) {
        return false;
    }
    *reg_out = (uint8_t)(keycode - KC_A);
    return true;
}

static bool blink_on(uint16_t period_ms) {
    return ((timer_read32() / (period_ms / 2)) & 1) != 0;
}

void vim_macro_on_vim_disable(void) {
    state           = VIM_MACRO_IDLE;
    active_reg      = 0;
    skip_capture    = false;
    pending_reg_swallow = 0xFF;
    pool_used       = 0;
    playback.active = false;
    memset(reg_offset, 0, sizeof(reg_offset));
    memset(reg_count, 0, sizeof(reg_count));
    memset(pool, 0, sizeof(pool));
    macro_dot_ready = false;
    macro_dot_reg   = 0;
}

bool vim_macro_is_recording(void) {
    return state == VIM_MACRO_RECORDING;
}

bool vim_macro_is_playing(void) {
    return playback.active;
}

bool vim_macro_is_injecting(void) {
    return playback.active && playback.injecting;
}

static void start_recording(uint8_t reg) {
    active_reg            = reg;
    reg_offset[reg]       = pool_used;
    reg_count[reg]        = 0;
    state                 = VIM_MACRO_RECORDING;
    skip_capture          = true;
    pending_reg_swallow   = reg;
}

static void stop_recording(void) {
    if (state == VIM_MACRO_RECORDING && reg_count[active_reg] == 0) {
        pool_used = reg_offset[active_reg];
    }
    state      = VIM_MACRO_IDLE;
    active_reg = 0;
}

static bool reg_has_macro(uint8_t reg) {
    return reg < VIM_MACRO_REG_COUNT && reg_count[reg] > 0;
}

static void queue_playback(uint8_t reg, int16_t count) {
    if (!reg_has_macro(reg)) {
        return;
    }
    if (count < 1) {
        count = 1;
    }
    playback.active       = true;
    playback.reg          = reg;
    playback.repeats_left = count;
    playback.index        = 0;
    playback.next_time    = timer_read32();
    macro_dot_ready       = true;
    macro_dot_reg         = reg;
}

void vim_repeat_action_recorded(void) {
    macro_dot_ready = false;
}

void vim_macro_stop_playback(void) {
    if (playback.active) {
        clear_keyboard();
    }
    playback.active = false;
}

bool vim_macro_process_normal(uint16_t keycode, const keyrecord_t *record) {
    if (!vim_mode_enabled()) {
        return true;
    }

    if (macro_is_recording()) {
        return true;
    }

    if (playback.active && !playback.injecting) {
        if (record->event.pressed && keycode == KC_ESC) {
            vim_macro_stop_playback();
        }
        return false;
    }

    if (pending_reg_swallow != 0xFF) {
        uint8_t reg;
        if (is_reg_letter(keycode, &reg) && reg == pending_reg_swallow && !record->event.pressed) {
            pending_reg_swallow = 0xFF;
            return false;
        }
    }

    if (record->event.pressed && keycode == KC_ESC) {
        if (state == VIM_MACRO_WAIT_REG || state == VIM_MACRO_WAIT_PLAY) {
            state = VIM_MACRO_IDLE;
            return false;
        }
        if (state == VIM_MACRO_RECORDING) {
            reg_count[active_reg] = 0;
            pool_used             = reg_offset[active_reg];
            stop_recording();
            return false;
        }
    }

    if (state == VIM_MACRO_RECORDING && get_vim_mode() == NORMAL_MODE && keycode == KC_Q) {
        if (record->event.pressed) {
            stop_recording();
            return false;
        }
        return true;
    }

    if (!record->event.pressed) {
        return true;
    }

    if (state == VIM_MACRO_WAIT_REG) {
        uint8_t reg;
        if (is_reg_letter(keycode, &reg)) {
            start_recording(reg);
            return false;
        }
        state = VIM_MACRO_IDLE;
        return true;
    }

    if (state == VIM_MACRO_WAIT_PLAY) {
        int16_t count    = motion_counter > 0 ? motion_counter : 1;
        motion_counter   = 0;

        if (is_at_key(keycode)) {
            queue_playback(last_play_reg, count);
            state = VIM_MACRO_IDLE;
            return false;
        }

        uint8_t reg;
        if (is_reg_letter(keycode, &reg)) {
            last_play_reg       = reg;
            pending_reg_swallow = reg;
            queue_playback(reg, count);
            state = VIM_MACRO_IDLE;
            return false;
        }

        state = VIM_MACRO_IDLE;
        return true;
    }

    if (state == VIM_MACRO_IDLE && keycode == KC_Q) {
        state = VIM_MACRO_WAIT_REG;
        return false;
    }

    if (state == VIM_MACRO_IDLE && record->event.pressed && keycode == KC_DOT) {
        if (macro_dot_ready && reg_has_macro(macro_dot_reg)) {
            int16_t count  = motion_counter > 0 ? motion_counter : 1;
            motion_counter = 0;
            queue_playback(macro_dot_reg, count);
            return false;
        }
        return true;
    }

    if (state == VIM_MACRO_IDLE && is_at_key(keycode)) {
        state = VIM_MACRO_WAIT_PLAY;
        return false;
    }

    return true;
}

void vim_macro_capture_vim_key(uint16_t keycode, const keyrecord_t *record, bool passed_through) {
    (void)passed_through;

    if (state != VIM_MACRO_RECORDING || playback.active) {
        return;
    }

    if (skip_capture) {
        skip_capture = false;
        return;
    }

    if (!record->event.pressed) {
        return;
    }

    if (pool_used >= VIM_MACRO_POOL_SIZE) {
        stop_recording();
        return;
    }

    pool[pool_used++] = (vim_macro_event_t){
        .keycode = keycode,
        .row     = record->event.key.row,
        .col     = record->event.key.col,
    };
    reg_count[active_reg]++;
}

void vim_macro_task(void) {
    if (!playback.active || !vim_mode_enabled()) {
        playback.active = false;
        return;
    }

    uint32_t now = timer_read32();
    if ((int32_t)(now - playback.next_time) < 0) {
        return;
    }

    uint8_t count = reg_count[playback.reg];
    if (playback.index >= count) {
        playback.repeats_left--;
        if (playback.repeats_left > 0) {
            playback.index     = 0;
            playback.next_time = now + 2;
            return;
        }
        macro_dot_ready = true;
        macro_dot_reg   = playback.reg;
        clear_keyboard();
        playback.active = false;
        return;
    }

    vim_macro_event_t *ev = &pool[reg_offset[playback.reg] + playback.index];
    playback.playback_record.event.key.row = ev->row;
    playback.playback_record.event.key.col = ev->col;
    playback.playback_record.event.pressed = true;
    playback.playback_record.event.time    = timer_read();

    playback.injecting = true;
    bool pass          = process_vim_mode(ev->keycode, &playback.playback_record);
    if (!pass) {
        /* Motions (j/k/h/l, etc.) register on press and unregister on release.
         * Recording only stores presses, so replay the release too. */
        wait_ms(TAP_CODE_DELAY);
        playback.playback_record.event.pressed = false;
        process_vim_mode(ev->keycode, &playback.playback_record);
    } else {
        register_code16(ev->keycode);
        wait_ms(TAP_CODE_DELAY);
        unregister_code16(ev->keycode);
    }
    playback.injecting = false;

    rgb_matrix_handle_key_event(ev->row, ev->col, true);
    rgb_matrix_handle_key_event(ev->row, ev->col, false);

    playback.index++;
    playback.next_time = now + 1;
}

bool vim_macro_render_sides(void) {
    if (state != VIM_MACRO_RECORDING) {
        return false;
    }

    uint8_t hue = 140;
    switch (get_vim_mode()) {
        case NORMAL_MODE:
            hue = 140;
            break;
        case INSERT_MODE:
            hue = 90;
            break;
        case VISUAL_MODE:
        case VISUAL_LINE_MODE:
            hue = 35;
            break;
        default:
            break;
    }

    HSV     hsv = rgb_matrix_get_hsv();
    RGB     rgb = hsv_to_rgb((HSV){hue, hsv.s, 0xFF});
    uint8_t on  = blink_on(500) ? 0xFF : 0x00;

    set_side_l_rgb_side(rgb.r, rgb.g, rgb.b);
    set_side_r_rgb_side(on, on, 0x00);
    return true;
}

void disable_vim_mode_user(void) {
    vim_macro_on_vim_disable();
}
