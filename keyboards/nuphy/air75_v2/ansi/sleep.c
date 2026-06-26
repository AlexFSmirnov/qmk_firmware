/*
Copyright 2023 @ Nuphy <https://nuphy.com/>
Copyright 2024 @ jincao1

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

/* Sleep state machine. Ported from jincao1's air75v2-sleep branch.
 *
 * Compared to upstream NuPhy:
 *  - Calls mcu_pwr.c's enter_deep_sleep() to push the STM32F072 into
 *    PWR_STOPMode + EXTI wake; idle current drops from milliamps to
 *    microamps, fixing the "battery dies overnight" bug.
 *  - In USB or wireless-with-charger-attached mode we only do a light
 *    sleep (LEDs off, MCU stays awake) - deep sleep on USB power has
 *    been observed to crash on wake, and chargers interrupt the MCU
 *    repeatedly which negates the savings.
 *  - The pre-existing user_config.sleep_enable boolean still gates all
 *    sleep behaviour; no extra UI / keycode is needed.
 *
 * `f_wakeup_prepare` is now cleared eagerly in `pre_process_record_kb`
 * (see ansi.c) so the very first key after wake-up is delivered with
 * minimal latency.
 */

#include "ansi.h"
#include "hal_usb.h"
#include "usb_main.h"
#include "mcu_pwr.h"
#include "rf_queue.h"

extern user_config_t   user_config;
extern DEV_INFO_STRUCT dev_info;
extern uint16_t        rf_linking_time;
extern uint32_t        no_act_time;
extern bool            f_goto_sleep;
extern bool            f_wakeup_prepare;

void side_rgb_set_color_all(uint8_t r, uint8_t g, uint8_t b);
void side_rgb_refresh(void);
void dev_sts_sync(void);

/* Visual indicator for deep sleep entry then hand off to mcu_pwr.c.
 * Returns once the MCU resumes from STOP mode. */
static void deep_sleep_handle(void) {
    pwr_side_led_on();
    wait_ms(50);
    side_rgb_set_color_all(0x99, 0x00, 0x00);
    side_rgb_refresh();
    wait_ms(500);

    /* Resync state once more so the wake keystroke is more likely to
     * survive across the suspend. */
    dev_sts_sync();

    enter_deep_sleep();
    exit_deep_sleep();

    /* Avoid an immediate re-sleep on the first wake tick. */
    no_act_time = 0;
}

void sleep_handle(void) {
    static uint32_t delay_step_timer     = 0;
    static uint8_t  usb_suspend_debounce = 0;
    static uint32_t rf_disconnect_time   = 0;

    /* 50ms interval */
    if (timer_elapsed32(delay_step_timer) < 50) return;
    delay_step_timer = timer_read32();

    /* sleep process */
    if (f_goto_sleep) {
        f_goto_sleep         = 0;
        rf_disconnect_time   = 0;
        rf_linking_time      = 0;
        usb_suspend_debounce = 0;

        if (!user_config.sleep_enable) {
            /* Sleep disabled by the user - never enter any sleep mode. */
            return;
        }

        /* Avoid deep sleep while charging wirelessly: chargers
         * periodically wake the MCU which causes a sleep-wake churn.
         * Take a light sleep instead so the user can still see LED
         * activity. */
        if (dev_info.link_mode < LINK_USB && (dev_info.rf_charge & 0x01)) {
            enter_light_sleep();
        }
        /* Avoid deep sleep while plugged in over USB - some hosts /
         * USB-C chargers report power loss on wake which crashes the
         * board. The user is presumably tethered anyway. */
        else if (dev_info.link_mode == LINK_USB) {
            enter_light_sleep();
        } else {
            deep_sleep_handle();
            return;
        }

        f_wakeup_prepare = 1;
    }

    /* sleep check, won't reach here on deep sleep. */
    if (f_goto_sleep || f_wakeup_prepare) return;

    if (dev_info.link_mode == LINK_USB) {
        if (USB_DRIVER.state == USB_SUSPENDED) {
            usb_suspend_debounce++;
            if (usb_suspend_debounce >= 20) {
                f_goto_sleep = 1;
            }
        } else {
            usb_suspend_debounce = 0;
        }
    } else if (no_act_time >= SLEEP_TIME_DELAY) {
        f_goto_sleep = 1;
    } else if (rf_linking_time >= LINK_TIMEOUT) {
        f_goto_sleep = 1;
    } else if (dev_info.rf_state == RF_DISCONNECT) {
        rf_disconnect_time++;
        if (rf_disconnect_time > 5 * 20) { /* 5 seconds */
            f_goto_sleep = 1;
        }
    } else if (dev_info.rf_state == RF_CONNECT) {
        rf_disconnect_time = 0;
    }
}
