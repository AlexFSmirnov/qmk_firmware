/*
Copyright 2023 NuPhy & jincao1

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "quantum.h"

#define EXTI_PORT_R0             EXTI_PortSourceGPIOC
#define EXTI_PORT_R1             EXTI_PortSourceGPIOC
#define EXTI_PORT_R2             EXTI_PortSourceGPIOA
#define EXTI_PORT_R3             EXTI_PortSourceGPIOA
#define EXTI_PORT_R4             EXTI_PortSourceGPIOA
#define EXTI_PORT_R5             EXTI_PortSourceGPIOA

#define EXTI_PIN_R0              14  // C14
#define EXTI_PIN_R1              15  // C15
#define EXTI_PIN_R2              0   // A0
#define EXTI_PIN_R3              1   // A1
#define EXTI_PIN_R4              2   // A2
#define EXTI_PIN_R5              3   // A3

void enter_light_sleep(void);
void exit_light_sleep(void);
void enter_deep_sleep(void);
void exit_deep_sleep(void);

void pwr_rgb_led_off(void);
void pwr_rgb_led_on(void);
void pwr_side_led_off(void);
void pwr_side_led_on(void);

bool is_rgb_led_on(void);
bool is_side_led_on(void);

void led_pwr_sleep_handle(void);
void led_pwr_wake_handle(void);

/* Periodically called from rgb_matrix_indicators_kb to auto power-off
 * RGB / side LED drivers when the matrix / side strip have been idle
 * (zero brightness / zero output) for a while. Saves a meaningful chunk
 * of battery when LEDs are turned off. */
void led_power_handle(void);

/* Activity counters for led_power_handle, incremented every 10ms in
 * timer_pro() and reset to 0 whenever the relevant LED group is told
 * to draw something. */
extern uint16_t rgb_led_last_act;
extern uint16_t side_led_last_act;
