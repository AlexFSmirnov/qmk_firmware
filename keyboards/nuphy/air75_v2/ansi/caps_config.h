#pragma once

#include "quantum.h"

#define CAPS_KEY_ROW 3
#define CAPS_KEY_COL 0

/* True when Caps Lock on the base layer activates vim navigation (default). */
bool caps_nav_is_default(void);

bool caps_config_process_record(uint16_t keycode, keyrecord_t *record);
void caps_config_render_indicators(void);
