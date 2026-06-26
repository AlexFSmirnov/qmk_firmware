/*
 * Runtime macros.
 *
 * 6 slots, each holding up to MACRO_EVENTS_PER_SLOT (128) keyboard events
 * (keycode + position + delay). Stored in EEPROM (within the user data
 * block) and persisted across reboots.
 *
 * Slot keys live on the macro layer (`MACRO_LAYER` = 6) on cols F7..F12:
 *   row 0, col 7..12  (F7..F12)  -> erase occupied slot              (red)
 *   row 1, col 7..12  (7..=)     -> record/save (empty) or play
 *                                   with delays (occupied)           (white/yellow)
 *   row 2, col 7..12  (U..])     -> play instantly                   (green)
 *   row 0, col 0  (Esc)          -> cancel an in-progress recording
 *
 * Empty slots only show the number-row record key (white).
 * Occupied slots show all three: red (erase), yellow (delayed), green (instant).
 * Brightness is synced to the RGB matrix brightness (rgb_matrix_get_val).
 *
 * While on the macro layer, NO key event is recorded into the active
 * macro (including the macro layer trigger key itself).
 */

#include "macros.h"
#include "utils.h"
#include "eeconfig.h"
#include <string.h>

_Static_assert(sizeof(macro_event_t) == 4, "macro_event_t size mismatch");
_Static_assert(sizeof(macro_slot_t) == 4 + 4 * MACRO_EVENTS_PER_SLOT, "macro_slot_t size mismatch");

/* EEPROM layout within the user data block:
 *   [0..7]    user_config_t
 *   [8..15]   padding
 *   [16..]    macro slots (6 * sizeof(macro_slot_t) = 6 * 516 = 3096 bytes)
 * Total: 16 + 3096 = 3112 bytes. EECONFIG_USER_DATA_SIZE = 3200 (headroom).
 */
#define MACRO_EEPROM_BASE 16

/* Shared by recording and playback (mutually exclusive). */
static macro_slot_t macro_work_buf;

static void *slot_eeprom_addr(uint8_t slot) {
    return (uint8_t *)EECONFIG_USER_DATABLOCK + MACRO_EEPROM_BASE + (uint32_t)slot * sizeof(macro_slot_t);
}

/* ---------- slot storage ---------- */

void macro_slot_read(uint8_t slot, macro_slot_t *out) {
    if (slot >= MACRO_SLOT_COUNT) {
        memset(out, 0, sizeof(*out));
        return;
    }
    if (!eeconfig_is_user_datablock_valid()) {
        memset(out, 0, sizeof(*out));
        return;
    }
    eeprom_read_block(out, slot_eeprom_addr(slot), sizeof(macro_slot_t));
    if (out->event_count > MACRO_EVENTS_PER_SLOT) {
        memset(out, 0, sizeof(*out));
    }
}

void macro_slot_write(uint8_t slot, const macro_slot_t *in) {
    if (slot >= MACRO_SLOT_COUNT) return;
    eeprom_update_block(in, slot_eeprom_addr(slot), sizeof(macro_slot_t));
    eeprom_update_dword(EECONFIG_USER, (EECONFIG_USER_DATA_VERSION));
}

void macro_slot_clear(uint8_t slot) {
    memset(&macro_work_buf, 0, sizeof(macro_work_buf));
    macro_slot_write(slot, &macro_work_buf);
}

bool macro_slot_is_occupied(uint8_t slot) {
    if (slot >= MACRO_SLOT_COUNT) return false;
    if (!eeconfig_is_user_datablock_valid()) return false;
    uint8_t count = 0;
    eeprom_read_block(&count, slot_eeprom_addr(slot), 1);
    return count > 0 && count <= MACRO_EVENTS_PER_SLOT;
}

/* ---------- recording state ---------- */

static struct {
    bool         active;
    uint8_t      slot;
    uint32_t     last_event_time;
} recording = { .active = false, .slot = 0xFF };

bool    macro_is_recording(void)    { return recording.active; }
uint8_t macro_recording_slot(void)  { return recording.active ? recording.slot : 0xFF; }

void macro_cancel_recording(void) {
    recording.active = false;
    recording.slot   = 0xFF;
}

static void recording_save_and_stop(void) {
    macro_slot_write(recording.slot, &macro_work_buf);
    recording.active = false;
    recording.slot   = 0xFF;
}

/* ---------- playback state ---------- */

static struct {
    bool         active;
    bool         instant;
    uint8_t      slot;
    uint8_t      index;
    uint32_t     next_time;
    uint32_t     last_press_time;
} playback = { .active = false, .slot = 0xFF };

bool    macro_is_playing(void)    { return playback.active; }
uint8_t macro_playing_slot(void)  { return playback.active ? playback.slot : 0xFF; }

void macro_stop_playback(void) {
    playback.active = false;
    playback.slot   = 0xFF;
}

void macro_play(uint8_t slot, bool instant) {
    if (recording.active) return;
    if (slot >= MACRO_SLOT_COUNT) return;
    macro_slot_read(slot, &macro_work_buf);
    if (macro_work_buf.event_count == 0) return;
    playback.active          = true;
    playback.instant         = instant;
    playback.slot            = slot;
    playback.index           = 0;
    playback.next_time       = timer_read32();
    playback.last_press_time = 0;
}

void macro_task(void) {
    if (!playback.active) return;

    uint32_t now = timer_read32();
    if ((int32_t)(now - playback.next_time) < 0) return;

    macro_event_t ev = macro_work_buf.events[playback.index];
    uint8_t pos     = ev.pos_pressed & MACRO_EVENT_POS_MASK;
    uint8_t row     = pos / MATRIX_COLS;
    uint8_t col     = pos % MATRIX_COLS;
    bool    pressed = (ev.pos_pressed & MACRO_EVENT_PRESSED) != 0;

    if (pressed) {
        register_code16(ev.keycode);
        playback.last_press_time = now;
    } else {
        unregister_code16(ev.keycode);
    }
    rgb_matrix_handle_key_event(row, col, pressed);

    playback.index++;
    if (playback.index >= macro_work_buf.event_count) {
        playback.active = false;
        playback.slot   = 0xFF;
        return;
    }

    uint32_t delay = playback.instant
                         ? 1
                         : (uint32_t)macro_work_buf.events[playback.index].delay_units * 10;
    if (delay < 1) delay = 1;
    playback.next_time = now + delay;
}

/* ---------- record button (Q..]) handler ---------- */

void macro_record_button_pressed(uint8_t slot) {
    if (slot >= MACRO_SLOT_COUNT) return;
    if (playback.active) return;

    if (recording.active) {
        if (recording.slot == slot) {
            recording_save_and_stop();
        }
        /* If recording into a different slot, ignore this press to avoid
         * accidental data loss. User can press Esc to cancel. */
        return;
    }

    if (macro_slot_is_occupied(slot)) {
        return;
    }

    /* Start recording. */
    memset(&macro_work_buf, 0, sizeof(macro_work_buf));
    recording.slot            = slot;
    recording.last_event_time = timer_read32();
    recording.active          = true;
}

/* ---------- position helpers ---------- */

static bool macro_col_is_slot(uint8_t col) {
    return col >= MACRO_SLOT_COL_MIN && col <= MACRO_SLOT_COL_MAX;
}

macro_kind_t macro_kind_for_pos(uint8_t row, uint8_t col) {
    if (row == 0 && col == 0) return MACRO_KIND_CANCEL;  /* Esc */
    if (!macro_col_is_slot(col)) return MACRO_KIND_NONE;
    switch (row) {
        case 0: return MACRO_KIND_ERASE;
        case 1: return MACRO_KIND_REC_OR_DELAYED;
        case 2: return MACRO_KIND_PLAY_INSTANT;
        default: return MACRO_KIND_NONE;
    }
}

uint8_t macro_slot_for_pos(uint8_t row, uint8_t col) {
    if (!macro_col_is_slot(col)) return 0xFF;
    if (row > 2) return 0xFF;
    return col - MACRO_SLOT_COL_MIN;
}

/* ---------- process_record integration ---------- */

static void capture_recorded_event(uint16_t keycode, keyrecord_t *record) {
    if (!recording.active) return;
    /* Never record anything that happens while the macro layer is active
     * (including the macro-layer activator itself). The macro layer is the
     * recording UI, not part of the macro. */
    if (layer_state_is(MACRO_LAYER)) return;
    if (keycode == MO(MACRO_LAYER)) return;
    if (macro_work_buf.event_count >= MACRO_EVENTS_PER_SLOT) {
        recording_save_and_stop();
        return;
    }

    uint32_t now   = timer_read32();
    uint32_t delta = macro_work_buf.event_count == 0 ? 0 : (now - recording.last_event_time);
    uint32_t units = delta / 10;
    if (units > 255) units = 255;

    uint8_t pos = (uint8_t)((record->event.key.row * MATRIX_COLS + record->event.key.col) & MACRO_EVENT_POS_MASK);
    if (record->event.pressed) pos |= MACRO_EVENT_PRESSED;

    macro_work_buf.events[macro_work_buf.event_count] = (macro_event_t){
        .keycode     = keycode,
        .pos_pressed = pos,
        .delay_units = (uint8_t)units,
    };
    macro_work_buf.event_count++;
    recording.last_event_time = now;

    if (macro_work_buf.event_count >= MACRO_EVENTS_PER_SLOT) {
        recording_save_and_stop();
    }
}

bool macro_process_record(uint16_t keycode, keyrecord_t *record) {
    bool on_macro_layer = layer_state_is(MACRO_LAYER);

    if (on_macro_layer) {
        /* Never swallow the layer activator itself - the layer engine needs
         * to see its release to deactivate. */
        if (keycode == MO(MACRO_LAYER)) return false;
        if (!record->event.pressed) return true;  /* only act on key-down */

        macro_kind_t kind = macro_kind_for_pos(record->event.key.row, record->event.key.col);
        uint8_t      slot = macro_slot_for_pos(record->event.key.row, record->event.key.col);

        switch (kind) {
            case MACRO_KIND_ERASE:
                if (macro_slot_is_occupied(slot)) {
                    macro_slot_clear(slot);
                }
                return true;
            case MACRO_KIND_REC_OR_DELAYED:
                if (macro_slot_is_occupied(slot)) {
                    if (!recording.active) macro_play(slot, false);
                } else {
                    macro_record_button_pressed(slot);
                }
                return true;
            case MACRO_KIND_PLAY_INSTANT:
                if (!recording.active) macro_play(slot, true);
                return true;
            case MACRO_KIND_CANCEL:
                if (recording.active) {
                    macro_cancel_recording();
                } else if (playback.active) {
                    macro_stop_playback();
                }
                return true;
            case MACRO_KIND_NONE:
            default:
                /* swallow other keys on the macro layer to avoid stray input */
                return true;
        }
    }

    /* Off macro layer: if recording, capture this event. The event still
     * flows through to the OS so the user gets immediate feedback. */
    capture_recorded_event(keycode, record);
    return false;  /* "false" = not consumed by macro logic */
}

/* ---------- visualisation ---------- */

static uint8_t pulse_value(uint16_t period_ms) {
    uint32_t t = timer_read32() % period_ms;
    if (t < period_ms / 2) return (uint8_t)((t * 510U) / period_ms);
    return (uint8_t)(((period_ms - t) * 510U) / period_ms);
}

static bool blink_on(uint16_t period_ms) {
    return ((timer_read32() / (period_ms / 2)) & 1) != 0;
}

bool macro_render_sides(void) {
    if (recording.active) {
        /* Red blink while recording. The side-aware helper scales by the
         * user's SIDE LED brightness (SIDE_VAI / SIDE_VAD), so this
         * indicator respects the existing brightness controls. */
        uint8_t on = blink_on(500) ? 0xFF : 0x00;
        set_sides_rgb_side(on, 0, 0);
        return true;
    }
    /* No side LED override during playback - the matrix reactive effect
     * already provides per-keypress feedback, and the user explicitly
     * doesn't want the sides flashing during playback. */
    return false;
}

void macro_render_indicators(void) {
    uint8_t v = rgb_matrix_get_val();

    for (uint8_t i = 0; i < MACRO_SLOT_COUNT; i++) {
        bool occupied = macro_slot_is_occupied(i);
        uint8_t col     = i + MACRO_SLOT_COL_MIN;

        if (recording.active && recording.slot == i) {
            /* Recording into this slot: blink the number-row record key red. */
            uint8_t b = blink_on(400) ? 0xFF : 0x10;
            set_key_rgb(1, col, scale8(b, v), 0, 0);
            continue;
        }

        if (!occupied) {
            /* Empty slot: paint only the number-row record key white. */
            set_key_rgb(1, col, scale8(0xFF, v), scale8(0xFF, v), scale8(0xFF, v));
            continue;
        }

        /* Occupied slot:
         *   row 0 (F7..F12)  - erase           -> red
         *   row 1 (7..=)     - play delayed    -> yellow
         *   row 2 (U..])     - play instant    -> green
         */
        uint8_t fr = 0xFF, fg = 0,    fb = 0;   /* red    */
        uint8_t nr = 0xFF, ng = 0xC0, nb = 0;   /* yellow */
        uint8_t ur = 0,    ug = 0xFF, ub = 0;   /* green  */

        /* Pulse the active play key while this slot is playing. */
        if (playback.active && playback.slot == i) {
            uint8_t p     = pulse_value(600);
            uint8_t scale = 0x80 + (p >> 1);
            if (playback.instant) {
                ug = scale;
            } else {
                nr = scale;
                ng = scale8(0xC0, scale);
            }
        }

        set_key_rgb(0, col, scale8(fr, v), scale8(fg, v), scale8(fb, v));
        set_key_rgb(1, col, scale8(nr, v), scale8(ng, v), scale8(nb, v));
        set_key_rgb(2, col, scale8(ur, v), scale8(ug, v), scale8(ub, v));
    }

    /* Esc: red while a recording is in progress (acts as "cancel"). */
    if (recording.active) {
        set_key_rgb(0, 0, scale8(0xFF, v), 0, 0);
    }
}
