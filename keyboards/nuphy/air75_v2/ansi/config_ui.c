/*
 * Config-layer UI: sleep mode/time controls and on-keyboard feedback.
 */

#include "config_ui.h"
#include "ansi.h"
#include "layers.h"
#include "utils.h"
#include "timer.h"
#include <string.h>

extern user_config_t user_config;
extern bool          f_sleep_show;

/* Idle timeout presets in 10 ms ticks (matches no_act_time in ansi.c). */
static const uint32_t sleep_time_presets[] PROGMEM = {
    30 * 100,                 /* 30 s */
    60 * 100,                 /* 1 m */
    5 * 60 * 100,             /* 5 m */
    10 * 60 * 100,            /* 10 m */
    15 * 60 * 100,            /* 15 m */
    30 * 60 * 100,            /* 30 m */
    60 * 60 * 100,            /* 1 h */
    2 * 60 * 60 * 100,        /* 2 h */
    4 * 60 * 60 * 100,        /* 4 h */
    8 * 60 * 60 * 100,        /* 8 h */
    12 * 60 * 60 * 100,       /* 12 h */
    24 * 60 * 60 * 100,       /* 24 h */
};
#define SLEEP_TIME_PRESET_COUNT ((uint8_t)(sizeof(sleep_time_presets) / sizeof(sleep_time_presets[0])))
#define SLEEP_TIME_DEFAULT_IDX  6 /* 1 h - matches the old SLEEP_TIME_DELAY */

#define SLEEP_TIME_SHOW_MS 3000

static uint32_t sleep_time_show_timer = 0;
static bool     sleep_time_show_flag  = false;

static uint8_t clamp_sleep_time_idx(uint8_t idx) {
    if (idx >= SLEEP_TIME_PRESET_COUNT) {
        return SLEEP_TIME_DEFAULT_IDX;
    }
    return idx;
}

void config_user_data_migrate(void) {
    if (user_config.sleep_cfg_magic == SLEEP_CFG_MAGIC) {
        user_config.sleep_time_idx = clamp_sleep_time_idx(user_config.sleep_time_idx);
        if (user_config.sleep_mode > SLEEP_MODE_DEEP) {
            user_config.sleep_mode = SLEEP_MODE_DEEP;
        }
        return;
    }

    /* Legacy sleep_enable (0/1) -> 3-state sleep_mode. */
    user_config.sleep_mode = user_config.sleep_mode ? SLEEP_MODE_DEEP : SLEEP_MODE_OFF;
    user_config.sleep_time_idx = SLEEP_TIME_DEFAULT_IDX;
    user_config.sleep_cfg_magic = SLEEP_CFG_MAGIC;
    user_config_save();
}

uint32_t config_sleep_time_delay(void) {
    uint8_t idx = clamp_sleep_time_idx(user_config.sleep_time_idx);
    return pgm_read_dword(&sleep_time_presets[idx]);
}

uint8_t config_sleep_mode(void) {
    return user_config.sleep_mode;
}

static void sleep_time_show_trigger(void) {
    sleep_time_show_timer = timer_read32();
    sleep_time_show_flag  = true;
}

static void sleep_time_step(int8_t delta) {
    int16_t idx = (int16_t)user_config.sleep_time_idx + delta;
    if (idx < 0) {
        idx = 0;
    } else if (idx >= SLEEP_TIME_PRESET_COUNT) {
        idx = SLEEP_TIME_PRESET_COUNT - 1;
    }
    if ((uint8_t)idx == user_config.sleep_time_idx) {
        return;
    }
    user_config.sleep_time_idx = (uint8_t)idx;
    user_config_save();
    sleep_time_show_trigger();
}

static void sleep_mode_cycle(void) {
    if (user_config.sleep_mode > SLEEP_MODE_OFF) {
        user_config.sleep_mode--;
    } else {
        user_config.sleep_mode = SLEEP_MODE_DEEP;
    }
    f_sleep_show = true;
    user_config_save();
}

static uint8_t digit_col(char c) {
    if (c >= '1' && c <= '9') {
        return (uint8_t)(c - '0');
    }
    if (c == '0') {
        return 10;
    }
    return 255;
}

static void format_sleep_time(uint8_t idx, char *out, uint8_t out_len) {
    uint32_t seconds = pgm_read_dword(&sleep_time_presets[idx]) / 100;
    uint8_t  pos     = 0;

    if (seconds < 60) {
        if (seconds >= 10 && pos < out_len - 1) {
            out[pos++] = (char)('0' + (seconds / 10));
        }
        if (pos < out_len - 1) {
            out[pos++] = (char)('0' + (seconds % 10));
        }
        if (pos < out_len - 1) {
            out[pos++] = 's';
        }
    } else if (seconds < 3600) {
        uint32_t mins = seconds / 60;
        if (mins >= 10 && pos < out_len - 1) {
            out[pos++] = (char)('0' + (mins / 10));
        }
        if (pos < out_len - 1) {
            out[pos++] = (char)('0' + (mins % 10));
        }
        if (pos < out_len - 1) {
            out[pos++] = 'm';
        }
    } else {
        uint32_t hrs = seconds / 3600;
        if (hrs >= 10 && pos < out_len - 1) {
            out[pos++] = (char)('0' + (hrs / 10));
        }
        if (pos < out_len - 1) {
            out[pos++] = (char)('0' + (hrs % 10));
        }
        if (pos < out_len - 1) {
            out[pos++] = 'h';
        }
    }

    if (pos < out_len) {
        out[pos] = '\0';
    } else {
        out[out_len - 1] = '\0';
    }
}

static void paint_sleep_time_char(char c, uint8_t v) {
    const uint8_t gr = scale8(0x00, v);
    const uint8_t gg = scale8(0xFF, v);
    const uint8_t gb = scale8(0x40, v);

    uint8_t col = digit_col(c);
    if (col <= 10) {
        set_key_rgb(1, col, gr, gg, gb);
        return;
    }

    switch (c) {
        case 's':
            set_key_rgb(3, 2, gr, gg, gb);
            break;
        case 'm':
            set_key_rgb(4, 8, gr, gg, gb);
            break;
        case 'h':
            set_key_rgb(3, 6, gr, gg, gb);
            break;
        default:
            break;
    }
}

/* Overlay indicator greens/reds - use the same full-range values as layers.c
 * so scale8(component, rgb_matrix_get_val()) matches across keys. */
#define CFG_GRN_R 0x00
#define CFG_GRN_G 0xFF
#define CFG_GRN_B 0x00
#define CFG_RED_R 0xFF
#define CFG_RED_G 0x00
#define CFG_RED_B 0x00
#define CFG_YEL_R 0xFF
#define CFG_YEL_G 0x80
#define CFG_YEL_B 0x00

static void paint_key_scaled(uint8_t row, uint8_t col, uint8_t r, uint8_t g, uint8_t b, uint8_t v) {
    set_key_rgb(row, col, scale8(r, v), scale8(g, v), scale8(b, v));
}

static void paint_sleep_mode_key(uint8_t v) {
    switch (user_config.sleep_mode) {
        case SLEEP_MODE_OFF:
            paint_key_scaled(4, 2, CFG_RED_R, CFG_RED_G, CFG_RED_B, v);
            break;
        case SLEEP_MODE_LIGHT:
            paint_key_scaled(4, 2, CFG_YEL_R, CFG_YEL_G, CFG_YEL_B, v);
            break;
        case SLEEP_MODE_DEEP:
            paint_key_scaled(4, 2, CFG_GRN_R, CFG_GRN_G, CFG_GRN_B, v);
            break;
        default:
            break;
    }
}

void config_render_indicators(void) {
    if (IS_LAYER_ON(CONFIG_LAYER)) {
        paint_sleep_mode_key(rgb_matrix_get_val());
    }

    if (!sleep_time_show_flag) {
        return;
    }

    if (timer_elapsed32(sleep_time_show_timer) >= SLEEP_TIME_SHOW_MS) {
        sleep_time_show_flag = false;
        return;
    }

    char     label[8];
    uint8_t  idx = clamp_sleep_time_idx(user_config.sleep_time_idx);
    uint8_t  v   = rgb_matrix_get_val();

    format_sleep_time(idx, label, sizeof(label));
    for (uint8_t i = 0; label[i] != '\0'; i++) {
        paint_sleep_time_char(label[i], v);
    }
}

bool config_process_record(uint16_t keycode, keyrecord_t *record) {
    if (!record->event.pressed) {
        return true;
    }

    switch (keycode) {
        case SLEEP_MODE:
            sleep_mode_cycle();
            return false;

        case CFG_SLEEP_TM_UP:
            sleep_time_step(1);
            return false;

        case CFG_SLEEP_TM_DN:
            sleep_time_step(-1);
            return false;

        default:
            return true;
    }
}

void config_task(void) {
    /* Reserved for future periodic config UI work. */
}
