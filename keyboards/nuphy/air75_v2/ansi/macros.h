#pragma once

#include "quantum.h"

#define MACRO_LAYER             6
#define MACRO_SLOT_COUNT        12
#define MACRO_EVENTS_PER_SLOT   64

/* Short alias to keep keymap MO() calls compact (see layers.h for the rest). */
#define L_MCR  MACRO_LAYER

/* Single recorded event. 4 bytes (no padding):
 *   keycode      - the resolved keycode (as seen by process_record_user)
 *   pos_pressed  - bit 7 = pressed, bits 0..6 = row*MATRIX_COLS + col
 *   delay_units  - delay BEFORE this event, in 10ms units (0..2550ms).
 *                  delay_units of the first event is 0 (plays immediately).
 */
typedef struct __attribute__((packed)) {
    uint16_t keycode;
    uint8_t  pos_pressed;
    uint8_t  delay_units;
} macro_event_t;

#define MACRO_EVENT_PRESSED   0x80
#define MACRO_EVENT_POS_MASK  0x7F

/* One macro slot. 4 byte header + 64*4 byte events = 260 bytes. */
typedef struct __attribute__((packed)) {
    uint8_t       event_count;   /* 0 = empty; 1..MACRO_EVENTS_PER_SLOT = recorded */
    uint8_t       reserved[3];
    macro_event_t events[MACRO_EVENTS_PER_SLOT];
} macro_slot_t;

/* ----- Public API ----- */

/* Read/write a slot to/from EEPROM. */
void macro_slot_read(uint8_t slot, macro_slot_t *out);
void macro_slot_write(uint8_t slot, const macro_slot_t *in);
void macro_slot_clear(uint8_t slot);
bool macro_slot_is_occupied(uint8_t slot);

/* True if a recording is in progress. */
bool macro_is_recording(void);
/* Which slot, if any, is currently being recorded into (0..11 or 0xFF). */
uint8_t macro_recording_slot(void);

/* True if a playback is in progress. */
bool macro_is_playing(void);
/* Which slot is currently being played back (0..11 or 0xFF). */
uint8_t macro_playing_slot(void);

/* Toggle recording into `slot`:
 *   - empty slot     -> start recording
 *   - recording this -> stop and save
 *   - occupied slot  -> delete
 *   - busy elsewhere -> no-op
 */
void macro_record_button_pressed(uint8_t slot);

/* Cancel an in-progress recording without saving. */
void macro_cancel_recording(void);

/* Start playback. `instant=true` ignores recorded delays. */
void macro_play(uint8_t slot, bool instant);
/* Stop in-progress playback (does not release any held keys). */
void macro_stop_playback(void);

/* ----- Integration hooks (call from user.c) ----- */

/* Returns true if the event was consumed (caller should return false from
 * process_record_user). Handles:
 *   - while on macro layer: slot keys + cancel + ignore
 *   - while recording (off-layer): capture event into the active slot
 */
bool macro_process_record(uint16_t keycode, keyrecord_t *record);

/* Drives playback timing. Call from housekeeping_task_user(). */
void macro_task(void);

/* Returns the slot index (0..11) for the given matrix position when on the
 * macro layer, or 0xFF if the position is not a macro slot key. */
uint8_t macro_slot_for_pos(uint8_t row, uint8_t col);

/* Action kind for a position on the macro layer. */
typedef enum {
    MACRO_KIND_NONE = 0,
    MACRO_KIND_PLAY_DELAYED,  /* row 0: F1..F12 */
    MACRO_KIND_PLAY_INSTANT,  /* row 1: 1..= */
    MACRO_KIND_REC_DEL,       /* row 2: Q..] */
    MACRO_KIND_CANCEL,        /* row 0 col 0: Esc */
} macro_kind_t;

macro_kind_t macro_kind_for_pos(uint8_t row, uint8_t col);

/* Per-event visualisation hook for the side LEDs.
 * Returns true if the side LEDs should be painted by the macro system
 * (recording or playback indicator). */
bool macro_render_sides(void);

/* Per-slot indicator rendering (call inside rgb_matrix_indicators_user
 * when the macro layer is the active layer). Lights slot status colors. */
void macro_render_indicators(void);
