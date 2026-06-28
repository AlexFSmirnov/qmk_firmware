#pragma once

#include "quantum.h"

#define VIM_MACRO_REG_COUNT   26
/* Shared pool capacity (see vim_macros.c). */
#define VIM_MACRO_POOL_SIZE   64

/* Normal-mode hook (q / @). Return false to consume the key. */
bool vim_macro_process_normal(uint16_t keycode, const keyrecord_t *record);

/* Capture a key event while macro recording (call after process_vim_mode). */
void vim_macro_capture_record(uint16_t keycode, const keyrecord_t *record);

/* Called when vim mode is disabled; clears all register data. */
void vim_macro_on_vim_disable(void);

void vim_macro_task(void);

bool vim_macro_is_recording(void);
bool vim_macro_is_playing(void);
bool vim_macro_is_injecting(void);
void vim_macro_stop_playback(void);

/* Side LED: yellow blink on the right strip while recording. */
bool vim_macro_render_sides(void);
