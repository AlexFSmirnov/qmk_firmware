#pragma once

#include "quantum.h"

#define SLEEP_MODE_OFF   0
#define SLEEP_MODE_LIGHT 1
#define SLEEP_MODE_DEEP  2

#define SLEEP_CFG_MAGIC  0xA6

/* Migrate legacy sleep_enable layout and apply defaults. */
void config_user_data_migrate(void);

uint32_t config_sleep_time_delay(void);
uint8_t  config_sleep_mode(void);

bool config_process_record(uint16_t keycode, keyrecord_t *record);
void config_task(void);
void config_render_indicators(void);
