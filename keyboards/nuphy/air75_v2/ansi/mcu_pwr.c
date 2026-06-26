/*
Copyright 2023 NuPhy, Persama (@Persama) & jincao1

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

/* Power-management module ported from jincao1's air75v2-sleep branch.
 * - Tracks RGB and side LED driver power state so they only consume
 *   battery when actually rendering something.
 * - Implements proper STM32F072 deep-sleep (PWR_EnterSTOPMode) with EXTI
 *   matrix wake-up, dropping the board's idle current from milliamps to
 *   microamps.
 *
 * Compared to jincao's version this file:
 * - Drops the 3-state SLEEP_MODE_* (deep/light/off) - the user keeps the
 *   stock single sleep_enable boolean. The caller (sleep.c) decides.
 * - Drops the f_usb_deinit / TIM6 idle-sleep paths (commented out in the
 *   original as causing wake issues on USB-C chargers).
 * - Uses the local globals (no kb_config / side_led_last_act extern from
 *   user_kb.c) - they live here. */

#include "ansi.h"
#include "mcu_stm32f0xx.h"
#include "mcu_pwr.h"
#include "hal_usb.h"
#include "usb_main.h"
#include "hal_lld.h"

uint16_t rgb_led_last_act  = 0;
uint16_t side_led_last_act = 0;

static const pin_t row_pins[MATRIX_ROWS] = MATRIX_ROW_PINS;
static const pin_t col_pins[MATRIX_COLS] = MATRIX_COL_PINS;

extern DEV_INFO_STRUCT dev_info;
extern user_config_t   user_config;

static bool sleeping             = false;
static bool side_led_powered_off = false;
static bool rgb_led_powered_off  = false;

static bool rgb_led_on  = false;
static bool side_led_on = false;

/* Forward declarations (live in rf.c / side.c). */
void clear_report_buffer_and_queue(void);
void side_rgb_set_color_all(uint8_t r, uint8_t g, uint8_t b);
void rgb_matrix_update_pwm_buffers(void);

#define EXTI_PortSourceGPIOA ((uint8_t)0x00)
#define EXTI_PortSourceGPIOB ((uint8_t)0x01)
#define EXTI_PortSourceGPIOC ((uint8_t)0x02)
#define EXTI_PortSourceGPIOD ((uint8_t)0x03)

void SYSCFG_EXTILineConfig(uint8_t EXTI_PortSourceGPIOx, uint8_t EXTI_PinSourcex) {
    uint32_t tmp = ((uint32_t)0x0F) << (0x04 * (EXTI_PinSourcex & (uint8_t)0x03));
    SYSCFG->EXTICR[EXTI_PinSourcex >> 0x02] &= ~tmp;
    SYSCFG->EXTICR[EXTI_PinSourcex >> 0x02] |= (((uint32_t)EXTI_PortSourceGPIOx) << (0x04 * (EXTI_PinSourcex & (uint8_t)0x03)));
}

void EXTI_ClearFlag(uint32_t EXTI_Line) {
    EXTI->PR = EXTI_Line;
}

void EXTI_DeInit(void) {
    EXTI->IMR  = 0x0F940000;
    EXTI->EMR  = 0x00000000;
    EXTI->RTSR = 0x00000000;
    EXTI->FTSR = 0x00000000;
    EXTI->PR   = 0x006BFFFF;
}

void EXTI_Init(EXTI_InitTypeDef *EXTI_InitStruct) {
    uint32_t tmp = (uint32_t)EXTI_BASE;

    if (EXTI_InitStruct->EXTI_LineCmd != DISABLE) {
        EXTI->IMR &= ~EXTI_InitStruct->EXTI_Line;
        EXTI->EMR &= ~EXTI_InitStruct->EXTI_Line;

        tmp += EXTI_InitStruct->EXTI_Mode;
        *(__IO uint32_t *)tmp |= EXTI_InitStruct->EXTI_Line;

        EXTI->RTSR &= ~EXTI_InitStruct->EXTI_Line;
        EXTI->FTSR &= ~EXTI_InitStruct->EXTI_Line;

        if (EXTI_InitStruct->EXTI_Trigger == EXTI_Trigger_Rising_Falling) {
            EXTI->RTSR |= EXTI_InitStruct->EXTI_Line;
            EXTI->FTSR |= EXTI_InitStruct->EXTI_Line;
        } else {
            tmp = (uint32_t)EXTI_BASE;
            tmp += EXTI_InitStruct->EXTI_Trigger;
            *(__IO uint32_t *)tmp |= EXTI_InitStruct->EXTI_Line;
        }
    } else {
        tmp += EXTI_InitStruct->EXTI_Mode;
        *(__IO uint32_t *)tmp &= ~EXTI_InitStruct->EXTI_Line;
    }
}

void EXTI_StructInit(EXTI_InitTypeDef *EXTI_InitStruct) {
    EXTI_InitStruct->EXTI_Line    = 0;
    EXTI_InitStruct->EXTI_Mode    = EXTI_Mode_Interrupt;
    EXTI_InitStruct->EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStruct->EXTI_LineCmd = DISABLE;
}

#define STM32_EXTI_0_1_HANDLER  Vector54
#define STM32_EXTI_2_3_HANDLER  Vector58
#define STM32_EXTI_4_15_HANDLER Vector5C

OSAL_IRQ_HANDLER(STM32_EXTI_0_1_HANDLER) {
    EXTI->PR = 0xffff;
}

OSAL_IRQ_HANDLER(STM32_EXTI_2_3_HANDLER) {
    EXTI->PR = 0xffff;
}

OSAL_IRQ_HANDLER(STM32_EXTI_4_15_HANDLER) {
    EXTI->PR = 0xffff;
}

/* ---------- LED driver power ---------- */

void pwr_rgb_led_off(void) {
    if (!rgb_led_on) return;
    gpio_set_pin_output(DC_BOOST_PIN);
    gpio_write_pin_low(DC_BOOST_PIN);
    gpio_set_pin_input(DRIVER_LED_CS_PIN);
    wait_us(200);
    rgb_led_on = false;
}

void pwr_rgb_led_on(void) {
    if (sleeping || rgb_led_on) return;
    gpio_set_pin_output(DC_BOOST_PIN);
    gpio_write_pin_high(DC_BOOST_PIN);
    gpio_set_pin_output(DRIVER_LED_CS_PIN);
    gpio_write_pin_low(DRIVER_LED_CS_PIN);
    wait_us(200);
    rgb_led_on = true;
}

void pwr_side_led_off(void) {
    if (!side_led_on) return;
    gpio_set_pin_input(DRIVER_SIDE_CS_PIN);
    wait_us(200);
    side_led_on = false;
}

void pwr_side_led_on(void) {
    if (sleeping || side_led_on) return;
    gpio_set_pin_output(DRIVER_SIDE_CS_PIN);
    gpio_write_pin_low(DRIVER_SIDE_CS_PIN);
    wait_us(200);
    side_led_on = true;
}

bool is_rgb_led_on(void) {
    return rgb_led_on;
}

bool is_side_led_on(void) {
    return side_led_on;
}

void led_pwr_sleep_handle(void) {
    side_led_powered_off = false;
    rgb_led_powered_off  = false;

    if (is_rgb_led_on()) {
        rgb_led_powered_off = true;
        pwr_rgb_led_off();
    }
    if (is_side_led_on()) {
        side_led_powered_off = true;
        pwr_side_led_off();
    }
}

void led_pwr_wake_handle(void) {
    if (rgb_led_powered_off) {
        pwr_rgb_led_on();
        /* Force a buffer flush after power-up. The WS2812 driver skips
         * the DMA if the colour set matches the previous frame. */
        rgb_matrix_set_color_all(0, 0, 0);
    }
    if (side_led_powered_off) {
        pwr_side_led_on();
        side_rgb_set_color_all(0, 0, 0);
    }
}

/* Called every ~500ms from rgb_matrix_indicators_kb (via ansi.c).
 * Auto-disables the LED drivers when their respective groups have been
 * idle / dark for > 1s. */
void led_power_handle(void) {
    static uint32_t interval = 0;

    if (timer_elapsed32(interval) < 500) return;
    interval = timer_read32();

    if (rgb_led_last_act > 100) { /* >1s since last paint */
        if (rgb_matrix_is_enabled() && rgb_matrix_get_val() != 0) {
            pwr_rgb_led_on();
        } else {
            pwr_rgb_led_off();
        }
    }

    if (side_led_last_act > 100) {
        extern uint8_t side_light;
        if (side_light == 0) {
            pwr_side_led_off();
        } else {
            pwr_side_led_on();
        }
    }
}

/* ---------- deep sleep ---------- */

void enter_deep_sleep(void) {
    if (dev_info.rf_state == RF_CONNECT)
        uart_send_cmd(CMD_SET_CONFIG, 5, 5);
    else
        uart_send_cmd(CMD_SLEEP, 5, 5);

    for (int i = 0; i < (int)ARRAY_SIZE(col_pins); i++) {
        gpio_set_pin_output(col_pins[i]);
        gpio_write_pin_high(col_pins[i]);
    }
    for (int i = 0; i < (int)ARRAY_SIZE(row_pins); i++) {
        gpio_set_pin_input_low(row_pins[i]);
    }

    SYSCFG_EXTILineConfig(EXTI_PORT_R0, EXTI_PIN_R0);
    SYSCFG_EXTILineConfig(EXTI_PORT_R1, EXTI_PIN_R1);
    SYSCFG_EXTILineConfig(EXTI_PORT_R2, EXTI_PIN_R2);
    SYSCFG_EXTILineConfig(EXTI_PORT_R3, EXTI_PIN_R3);
    SYSCFG_EXTILineConfig(EXTI_PORT_R4, EXTI_PIN_R4);
    SYSCFG_EXTILineConfig(EXTI_PORT_R5, EXTI_PIN_R5);

    EXTI_InitTypeDef m_exti;
    EXTI_StructInit(&m_exti);
    m_exti.EXTI_Line    = 0XFFFF;
    m_exti.EXTI_LineCmd = ENABLE;
    m_exti.EXTI_Mode    = EXTI_Mode_Interrupt;
    m_exti.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_Init(&m_exti);

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel         = EXTI4_15_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd      = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    NVIC_InitStructure.NVIC_IRQChannel = EXTI0_1_IRQn;
    NVIC_Init(&NVIC_InitStructure);
    NVIC_InitStructure.NVIC_IRQChannel = EXTI2_3_IRQn;
    NVIC_Init(&NVIC_InitStructure);

    led_pwr_sleep_handle();

    gpio_set_pin_output(DEV_MODE_PIN);
    gpio_write_pin_low(DEV_MODE_PIN);
    gpio_set_pin_output(SYS_MODE_PIN);
    gpio_write_pin_low(SYS_MODE_PIN);

    /* Any remaining LED-adjacent pins held low. */
    gpio_set_pin_output(A7);
    gpio_write_pin_low(A7);
    gpio_set_pin_output(DRIVER_SIDE_PIN);
    gpio_write_pin_low(DRIVER_SIDE_PIN);

    gpio_set_pin_output(NRF_TEST_PIN);
    gpio_write_pin_high(NRF_TEST_PIN);
    gpio_set_pin_output(NRF_WAKEUP_PIN);
    gpio_write_pin_high(NRF_WAKEUP_PIN);

    clear_report_buffer_and_queue();

    PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);
}

void exit_deep_sleep(void) {
    extern void matrix_init_pins(void);
    matrix_init_pins();

    gpio_set_pin_input_high(DEV_MODE_PIN);
    gpio_set_pin_input_high(SYS_MODE_PIN);

    gpio_set_pin_output(NRF_WAKEUP_PIN);

    led_pwr_wake_handle();

    stm32_clock_init();

    uart_send_cmd(CMD_HAND, 0, 1);

    /* Flag for the RF state machine that we just came back so it can
     * resend any queued keystrokes. */
    dev_info.rf_state = RF_WAKE;
}

/* ---------- light sleep (LEDs off, MCU stays running) ---------- */

void enter_light_sleep(void) {
    if (dev_info.rf_state == RF_CONNECT)
        uart_send_cmd(CMD_SET_CONFIG, 5, 5);
    else
        uart_send_cmd(CMD_SLEEP, 5, 5);

    led_pwr_sleep_handle();
    clear_report_buffer_and_queue();
    sleeping = true;
}

void exit_light_sleep(void) {
    sleeping = false;
    led_pwr_wake_handle();

    uart_send_cmd(CMD_HAND, 0, 1);

    if (dev_info.link_mode == LINK_USB) {
        usb_lld_wakeup_host(&USB_DRIVER);
        restart_usb_driver(&USB_DRIVER);
    }

    dev_info.rf_state = RF_WAKE;
}
