/*
 * Runtime macros.
 *
 * 12 slots, each holding up to MACRO_EVENTS_PER_SLOT (64) keyboard events
 * (keycode + position + delay). Stored in EEPROM (within the user data
 * block) and persisted across reboots.
 *
 * Slot keys live on the macro layer (`MACRO_LAYER` = 6) on the top 3 rows:
 *   row 0, col 1..12  (F1..F12)  -> play slot with original delays (yellow)
 *   row 1, col 1..12  (1..=)     -> play slot instantly             (green)
 *   row 2, col 1..12  (Q..])     -> record (empty, white) /
 *                                   save (currently recording, red blink) /
 *                                   delete (occupied, red)
 *   row 0, col 0  (Esc)          -> cancel an in-progress recording
 *
 * Empty slots only show the row-2 record key (white).
 * Occupied slots show all three: yellow (delayed), green (instant), red (erase).
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
 *   [16..]    macro slots (12 * sizeof(macro_slot_t) = 12 * 260 = 3120 bytes)
 * Total: 16 + 3120 = 3136 bytes. EECONFIG_USER_DATA_SIZE = 3200 (headroom).
 */
#define MACRO_EEPROM_BASE 16

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
    macro_slot_t empty = {0};
    macro_slot_write(slot, &empty);
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
    macro_slot_t buf;
} recording = { .active = false, .slot = 0xFF };

bool    macro_is_recording(void)    { return recording.active; }
uint8_t macro_recording_slot(void)  { return recording.active ? recording.slot : 0xFF; }

void macro_cancel_recording(void) {
    recording.active = false;
    recording.slot   = 0xFF;
}

static void recording_save_and_stop(void) {
    macro_slot_write(recording.slot, &recording.buf);
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
    macro_slot_t buf;
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
    macro_slot_read(slot, &playback.buf);
    if (playback.buf.event_count == 0) return;
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

    macro_event_t ev = playback.buf.events[playback.index];
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
    if (playback.index >= playback.buf.event_count) {
        playback.active = false;
        playback.slot   = 0xFF;
        return;
    }

    uint32_t delay = playback.instant
                         ? 1
                         : (uint32_t)playback.buf.events[playback.index].delay_units * 10;
    if (delay < 1) delay = 1;
    playback.next_time = now + delay;
}

/* ---------- record button (Q..]) handler ---------- */

void macro_record_button_pressed(uint8_t slot) {
    if (slot >= MACRO_SLOT_COUNT) return;

    if (recording.active) {
        if (recording.slot == slot) {
            recording_save_and_stop();
        }
        /* If recording into a different slot, ignore this press to avoid
         * accidental data loss. User can press Esc to cancel. */
        return;
    }

    if (macro_slot_is_occupied(slot)) {
        macro_slot_clear(slot);
        return;
    }

    /* Start recording. */
    memset(&recording.buf, 0, sizeof(recording.buf));
    recording.slot            = slot;
    recording.last_event_time = timer_read32();
    recording.active          = true;
}

/* ---------- position helpers ---------- */

macro_kind_t macro_kind_for_pos(uint8_t row, uint8_t col) {
    if (row == 0 && col == 0) return MACRO_KIND_CANCEL;  /* Esc */
    if (col < 1 || col > MACRO_SLOT_COUNT) return MACRO_KIND_NONE;
    switch (row) {
        case 0: return MACRO_KIND_PLAY_DELAYED;
        case 1: return MACRO_KIND_PLAY_INSTANT;
        case 2: return MACRO_KIND_REC_DEL;
        default: return MACRO_KIND_NONE;
    }
}

uint8_t macro_slot_for_pos(uint8_t row, uint8_t col) {
    if (col < 1 || col > MACRO_SLOT_COUNT) return 0xFF;
    if (row > 2) return 0xFF;
    return col - 1;
}

/* ---------- process_record integration ---------- */

static void capture_recorded_event(uint16_t keycode, keyrecord_t *record) {
    if (!recording.active) return;
    /* Never record anything that happens while the macro layer is active
     * (including the macro-layer activator itself). The macro layer is the
     * recording UI, not part of the macro. */
    if (layer_state_is(MACRO_LAYER)) return;
    if (keycode == MO(MACRO_LAYER)) return;
    if (recording.buf.event_count >= MACRO_EVENTS_PER_SLOT) {
        recording_save_and_stop();
        return;
    }

    uint32_t now   = timer_read32();
    uint32_t delta = recording.buf.event_count == 0 ? 0 : (now - recording.last_event_time);
    uint32_t units = delta / 10;
    if (units > 255) units = 255;

    uint8_t pos = (uint8_t)((record->event.key.row * MATRIX_COLS + record->event.key.col) & MACRO_EVENT_POS_MASK);
    if (record->event.pressed) pos |= MACRO_EVENT_PRESSED;

    recording.buf.events[recording.buf.event_count] = (macro_event_t){
        .keycode     = keycode,
        .pos_pressed = pos,
        .delay_units = (uint8_t)units,
    };
    recording.buf.event_count++;
    recording.last_event_time = now;

    if (recording.buf.event_count >= MACRO_EVENTS_PER_SLOT) {
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
            case MACRO_KIND_CANCEL:
                if (recording.active) {
                    macro_cancel_recording();
                } else if (playback.active) {
                    macro_stop_playback();
                }
                return true;
            case MACRO_KIND_PLAY_DELAYED:
                if (!recording.active) macro_play(slot, false);
                return true;
            case MACRO_KIND_PLAY_INSTANT:
                if (!recording.active) macro_play(slot, true);
                return true;
            case MACRO_KIND_REC_DEL:
                macro_record_button_pressed(slot);
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

        if (recording.active && recording.slot == i) {
            /* Recording into this slot: blink the record key red. The play
             * keys are left to the natural matrix effect (no overpaint). */
            uint8_t b = blink_on(400) ? 0xFF : 0x10;
            set_key_rgb(2, i + 1, scale8(b, v), 0, 0);
            continue;
        }

        if (!occupied) {
            /* Empty slot: paint only the record (Q-row) key white. F-row
             * and number-row keys are NOT overpainted, so the active RGB
             * matrix effect (solid reactive, etc.) keeps running there. */
            set_key_rgb(2, i + 1, scale8(0xFF, v), scale8(0xFF, v), scale8(0xFF, v));
            continue;
        }

        /* Occupied slot: paint all three indicator keys (full RGB; scaled
         * by matrix brightness `v` below).
         *   row 0 (F1..F12)  - play with original delays  -> yellow
         *   row 1 (1..=)     - play instantly             -> green
         *   row 2 (Q..])     - erase                      -> red
         */
        uint8_t fr = 0xFF, fg = 0xC0, fb = 0;   /* yellow */
        uint8_t nr = 0,    ng = 0xFF, nb = 0;   /* green  */
        uint8_t qr = 0xFF, qg = 0,    qb = 0;   /* red    */

        /* Pulse the active play key while this slot is playing. */
        if (playback.active && playback.slot == i) {
            uint8_t p     = pulse_value(600);
            uint8_t scale = 0x80 + (p >> 1);
            if (playback.instant) {
                ng = scale;
            } else {
                fr = scale;
                fg = scale8(0xC0, scale);
            }
        }

        set_key_rgb(0, i + 1, scale8(fr, v), scale8(fg, v), scale8(fb, v));
        set_key_rgb(1, i + 1, scale8(nr, v), scale8(ng, v), scale8(nb, v));
        set_key_rgb(2, i + 1, scale8(qr, v), scale8(qg, v), scale8(qb, v));
    }

    /* Esc: red while a recording is in progress (acts as "cancel"). */
    if (recording.active) {
        set_key_rgb(0, 0, scale8(0xFF, v), 0, 0);
    }
}
