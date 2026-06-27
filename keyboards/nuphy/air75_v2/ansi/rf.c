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

/* RF / UART driver. Ported from jincao1's air75v2-sleep branch.
 *
 * Key changes vs upstream NuPhy:
 *  - Variable-rate retransmission of the last keyboard report (report_buff_a
 *    + report_buff_b) to greatly reduce stuck / dropped keys on RF.
 *  - 64-entry FIFO `rf_queue` for events generated while disconnected, so a
 *    wake-up keystroke isn't lost while the link is reconnecting.
 *  - `uart_receive_pro` now waits a moment for the full UART burst before
 *    parsing it, and rejects bytes outside a valid frame; this fixes the
 *    most common RF crash where partial frames de-synced the parser.
 *  - `uart_send_bytes` rate-limits to one frame / ms and tracks the last
 *    send timestamp so the receive path can avoid clashing with TX.
 *  - Dropped the unused `CMD_SET_24G_NAME` blob and `uart_repeat_flag` triple
 *    send.
 */

#include "ansi.h"
#include "uart.h"
#include "rf_driver.h"
#include "rf_queue.h"

USART_MGR_STRUCT Usart_Mgr;
// clang-format off
#define RX_SBYTE Usart_Mgr.RXDBuf[0]
#define RX_CMD   Usart_Mgr.RXDBuf[1]
#define RX_ACK   Usart_Mgr.RXDBuf[2]
#define RX_LEN   Usart_Mgr.RXDBuf[3]
#define RX_DAT   Usart_Mgr.RXDBuf[4]
// clang-format on

bool f_uart_ack        = 0;
bool f_rf_read_data_ok = 0;
bool f_rf_sts_sysc_ok  = 0;
bool f_rf_new_adv_ok   = 0;
bool f_rf_reset        = 0;
bool f_rf_hand_ok      = 0;
bool f_goto_sleep      = 0;
bool f_wakeup_prepare  = 0;

uint8_t  func_tab[32]     = {0};
uint8_t  sync_lost        = 0;
uint8_t  disconnect_delay = 0;
uint32_t uart_rpt_timer   = 0;

/* Last-sent report cache. Retransmitted at a variable interval while the
 * physical key is still held so the receiver always converges to the
 * correct state even if individual UART frames are corrupted. */
report_buffer_t report_buff_a = {0};
report_buffer_t report_buff_b = {0};

extern DEV_INFO_STRUCT dev_info;
extern host_driver_t  *m_host_driver;
extern uint8_t         host_mode;
extern uint8_t         rf_blink_cnt;
extern uint16_t        rf_link_show_time;
extern uint16_t        rf_linking_time;
extern uint32_t        no_act_time;
extern bool            f_send_channel;
extern bool            f_dial_sw_init_ok;

void uart_init(uint32_t baud); // qmk uart.c
void uart_send_report(uint8_t report_type, uint8_t *report_buf, uint8_t report_size);
void uart_receive_pro(void);
void break_all_key(void);
uint8_t get_checksum(uint8_t *buf, uint8_t len);

/* ---------- retransmission helpers ---------- */

/* Step the inter-resend interval based on how many times the same report
 * has already been sent. Aggressive at first to compensate for missed
 * frames, then slow to a heartbeat once the link is settled. */
static uint8_t get_repeat_interval(void) {
    uint8_t interval = MAX(report_buff_a.repeat, report_buff_b.repeat);
    if (interval == 0)      return 50;
    else if (interval <= 4) return 8;
    else if (interval <= 7) return 12;
    else if (interval <= 10) return 14;
    return 25;
}

void clear_report_buffer(void) {
    if (report_buff_a.cmd) memset(&report_buff_a.cmd, 0, sizeof(report_buffer_t));
    if (report_buff_b.cmd) memset(&report_buff_b.cmd, 0, sizeof(report_buffer_t));
}

void clear_report_buffer_and_queue(void) {
    clear_report_buffer();
    rf_queue.clear();
}

/* Drain the wake-up queue. Each entry is sent and then repeated a few
 * times after a short delay before moving to the next. */
static void uart_send_repeat_from_queue(void) {
    static uint32_t        dequeue_timer = 0;
    static uint32_t        repeat_timer  = 0;
    static report_buffer_t report_buff   = {0};
    static bool            do_repeat     = true;

    if (timer_elapsed32(dequeue_timer) >= 30 && !rf_queue.is_empty()) {
        rf_queue.dequeue(&report_buff);
        dequeue_timer = timer_read32();
        /* Only keyboard reports benefit from straight retransmission -
         * mouse / consumer reports would replay the action. */
        do_repeat = (report_buff.cmd == CMD_RPT_BYTE_KB || report_buff.cmd == CMD_RPT_BIT_KB);
    }

    if (rf_queue.is_empty()) {
        clear_report_buffer_and_queue();
        if (do_repeat) report_buff_a = report_buff;
    }

    if (report_buff.repeat == 0 || (do_repeat && timer_elapsed32(repeat_timer) >= 3)) {
        uart_send_report(report_buff.cmd, report_buff.buffer, report_buff.length);
        report_buff.repeat++;
        repeat_timer = timer_read32();
    }
}

/* Called from the housekeeping loop. While the user is still holding the
 * keys, retransmit; once idle, drop the cached reports. */
void uart_send_report_repeat(void) {
    if (dev_info.link_mode == LINK_USB) return;

    if (dev_info.rf_state != RF_CONNECT) {
        /* 1s after the last activity, give up on the queue so we don't
         * blast random keys at the host once the link returns. */
        if (no_act_time > 100) clear_report_buffer_and_queue();
        return;
    }

    if (!rf_queue.is_empty()) {
        uart_send_repeat_from_queue();
        return;
    }

    uint8_t interval = get_repeat_interval();
    if (timer_elapsed32(uart_rpt_timer) >= interval) {
        if (no_act_time <= 25) { /* increments every 10ms - so 250ms key-hold window */
            if (report_buff_a.cmd) {
                uart_send_report(report_buff_a.cmd, report_buff_a.buffer, report_buff_a.length);
                report_buff_a.repeat++;
            }
            if (report_buff_b.cmd) {
                uart_send_report(report_buff_b.cmd, report_buff_b.buffer, report_buff_b.length);
                report_buff_b.repeat++;
            }
        } else {
            clear_report_buffer_and_queue();
        }
        uart_rpt_timer = timer_read32();
    }
}

/* ---------- RX path ---------- */

void rf_protocol_receive(void) {
    uint8_t i, check_sum = 0;

    if (Usart_Mgr.RXDState != RX_Done) return;

    sync_lost = 0;

    if (RX_LEN >= UART_MAX_LEN - 4) {
        /* Defensive: protect against an out-of-range length byte that
         * would otherwise walk off the end of the buffer. */
        Usart_Mgr.RXDState = RX_DATA_ERR;
        return;
    } else if (Usart_Mgr.RXDLen > 4) {
        for (i = 0; i < RX_LEN; i++) {
            check_sum += Usart_Mgr.RXDBuf[4 + i];
        }
        if (check_sum != Usart_Mgr.RXDBuf[4 + i]) {
            Usart_Mgr.RXDState = RX_SUM_ERR;
            return;
        }
    } else if (Usart_Mgr.RXDLen == 3) {
        if (Usart_Mgr.RXDBuf[2] == 0xA0) {
            f_uart_ack = 1;
        }
    }

    Usart_Mgr.RXCmd = RX_CMD;

    switch (RX_CMD) {
        case CMD_HAND: {
            f_rf_hand_ok = 1;
            break;
        }

        case CMD_24G_SUSPEND: {
            if (!dev_wireless_usb_powered(&dev_info)) {
                f_goto_sleep = 1;
            }
            break;
        }

        case CMD_NEW_ADV: {
            f_rf_new_adv_ok = 1;
            break;
        }

        case CMD_RF_STS_SYSC: {
            static uint8_t error_cnt = 0;

            if (dev_info.link_mode == Usart_Mgr.RXDBuf[4]) {
                error_cnt = 0;

                dev_info.rf_state = Usart_Mgr.RXDBuf[5];

                if ((dev_info.rf_state == RF_CONNECT) && ((Usart_Mgr.RXDBuf[6] & 0xf8) == 0)) {
                    dev_info.rf_led = Usart_Mgr.RXDBuf[6];
                }

                dev_info.rf_charge = Usart_Mgr.RXDBuf[7];

                if (Usart_Mgr.RXDBuf[8] <= 100) dev_info.rf_baterry = Usart_Mgr.RXDBuf[8];
                if (dev_info.rf_charge & 0x01) dev_info.rf_baterry = 100;
            } else {
                if (dev_info.rf_state != RF_INVALID) {
                    if (error_cnt >= 5) {
                        error_cnt      = 0;
                        f_send_channel = 1;
                    } else {
                        error_cnt++;
                    }
                }
            }

            f_rf_sts_sysc_ok = 1;
            break;
        }

        case CMD_READ_DATA: {
            memcpy(func_tab, &Usart_Mgr.RXDBuf[4], 32);

            if (func_tab[4] <= LINK_USB) {
                dev_info.link_mode = func_tab[4];
            }
            if (func_tab[5] < LINK_USB) {
                dev_info.rf_channel = func_tab[5];
            }
            if ((func_tab[6] <= LINK_BT_3) && (func_tab[6] >= LINK_BT_1)) {
                dev_info.ble_channel = func_tab[6];
            }

            f_rf_read_data_ok = 1;
            break;
        }
    }

    Usart_Mgr.RXDLen      = 0;
    Usart_Mgr.RXDState    = RX_Idle;
    Usart_Mgr.RXDOverTime = 0;
}

uint8_t uart_send_cmd(uint8_t cmd, uint8_t wait_ack, uint8_t delayms) {
    wait_ms(delayms);

    memset(&Usart_Mgr.TXDBuf[0], 0, UART_MAX_LEN);

    Usart_Mgr.TXDBuf[0] = UART_HEAD;
    Usart_Mgr.TXDBuf[1] = cmd;
    Usart_Mgr.TXDBuf[2] = 0x00;

    switch (cmd) {
        case CMD_SLEEP: {
            Usart_Mgr.TXDBuf[3] = 1;
            Usart_Mgr.TXDBuf[4] = 0;
            Usart_Mgr.TXDBuf[5] = 0;
            break;
        }
        case CMD_HAND: {
            Usart_Mgr.TXDBuf[3] = 1;
            Usart_Mgr.TXDBuf[4] = 0;
            Usart_Mgr.TXDBuf[5] = 0;
            break;
        }
        case CMD_RF_STS_SYSC: {
            Usart_Mgr.TXDBuf[3] = 1;
            Usart_Mgr.TXDBuf[4] = dev_info.link_mode;
            Usart_Mgr.TXDBuf[5] = dev_info.link_mode;
            break;
        }
        case CMD_SET_LINK: {
            dev_info.rf_state   = RF_LINKING;
            Usart_Mgr.TXDBuf[3] = 1;
            Usart_Mgr.TXDBuf[4] = dev_info.link_mode;
            Usart_Mgr.TXDBuf[5] = dev_info.link_mode;
            rf_linking_time  = 0;
            disconnect_delay = 0xff;
            break;
        }
        case CMD_NEW_ADV: {
            dev_info.rf_state   = RF_PAIRING;
            Usart_Mgr.TXDBuf[3] = 2;
            Usart_Mgr.TXDBuf[4] = dev_info.link_mode;
            Usart_Mgr.TXDBuf[5] = 1;
            Usart_Mgr.TXDBuf[6] = dev_info.link_mode + 1;
            rf_linking_time  = 0;
            disconnect_delay = 0xff;
            f_rf_new_adv_ok  = 0;
            break;
        }
        case CMD_CLR_DEVICE: {
            Usart_Mgr.TXDBuf[3] = 1;
            Usart_Mgr.TXDBuf[4] = 0;
            Usart_Mgr.TXDBuf[5] = 0;
            break;
        }
        case CMD_SET_CONFIG: {
            Usart_Mgr.TXDBuf[3] = 1;
            Usart_Mgr.TXDBuf[4] = POWER_DOWN_DELAY;
            Usart_Mgr.TXDBuf[5] = POWER_DOWN_DELAY;
            break;
        }
        case CMD_SET_NAME: {
            Usart_Mgr.TXDBuf[3]  = 17;
            Usart_Mgr.TXDBuf[4]  = 1;
            Usart_Mgr.TXDBuf[5]  = 15;
            Usart_Mgr.TXDBuf[6]  = 'N';
            Usart_Mgr.TXDBuf[7]  = 'u';
            Usart_Mgr.TXDBuf[8]  = 'P';
            Usart_Mgr.TXDBuf[9]  = 'h';
            Usart_Mgr.TXDBuf[10] = 'y';
            Usart_Mgr.TXDBuf[11] = ' ';
            Usart_Mgr.TXDBuf[12] = 'A';
            Usart_Mgr.TXDBuf[13] = 'i';
            Usart_Mgr.TXDBuf[14] = 'r';
            Usart_Mgr.TXDBuf[15] = '7';
            Usart_Mgr.TXDBuf[16] = '5';
            Usart_Mgr.TXDBuf[17] = ' ';
            Usart_Mgr.TXDBuf[18] = 'V';
            Usart_Mgr.TXDBuf[19] = '2';
            Usart_Mgr.TXDBuf[20] = '-';
            Usart_Mgr.TXDBuf[21] = get_checksum(Usart_Mgr.TXDBuf + 4, Usart_Mgr.TXDBuf[3]);
            break;
        }
        case CMD_READ_DATA: {
            Usart_Mgr.TXDBuf[3] = 2;
            Usart_Mgr.TXDBuf[4] = 0x00;
            Usart_Mgr.TXDBuf[5] = FUNC_VALID_LEN;
            Usart_Mgr.TXDBuf[6] = FUNC_VALID_LEN;
            break;
        }
        case CMD_RF_DFU: {
            Usart_Mgr.TXDBuf[3] = 1;
            Usart_Mgr.TXDBuf[4] = 0;
            Usart_Mgr.TXDBuf[5] = 0;
            break;
        }
        default:
            break;
    }

    f_uart_ack = 0;
    uart_send_bytes(Usart_Mgr.TXDBuf, Usart_Mgr.TXDBuf[3] + 5);

    if (wait_ack) {
        while (wait_ack--) {
            wait_ms(1);
            uart_receive_pro();
            if (f_uart_ack || Usart_Mgr.RXCmd == cmd) return TX_OK;
        }
    } else {
        return TX_OK;
    }

    return TX_TIMEOUT;
}

void dev_sts_sync(void) {
    static uint32_t interval_timer  = 0;
    static uint8_t  link_state_temp = RF_DISCONNECT;

    if (timer_elapsed32(interval_timer) < 200) return;
    interval_timer = timer_read32();

    if (f_rf_reset) {
        f_rf_reset = 0;
        wait_ms(100);
        gpio_write_pin_low(NRF_RESET_PIN);
        wait_ms(50);
        gpio_write_pin_high(NRF_RESET_PIN);
        wait_ms(50);
    } else if (f_send_channel) {
        f_send_channel = 0;
        uart_send_cmd(CMD_SET_LINK, 10, 10);
    }

    if (dev_info.link_mode == LINK_USB) {
        if (host_mode != HOST_USB_TYPE) {
            host_mode = HOST_USB_TYPE;
            break_all_key();
            host_set_driver(m_host_driver);
        }
        rf_blink_cnt = 0;
    } else {
        if (host_mode != HOST_RF_TYPE) {
            host_mode = HOST_RF_TYPE;
            break_all_key();
            host_set_driver(&rf_host_driver);
        }

        if (dev_info.rf_state != RF_CONNECT) {
            if (disconnect_delay >= 10) {
                rf_blink_cnt      = 3;
                rf_link_show_time = 0;
                link_state_temp   = dev_info.rf_state;
            } else {
                disconnect_delay++;
            }
        } else /* RF_CONNECT */ {
            rf_linking_time  = 0;
            disconnect_delay = 0;
            rf_blink_cnt     = 0;

            if (link_state_temp != RF_CONNECT) {
                link_state_temp   = RF_CONNECT;
                rf_link_show_time = 0;
            }
        }
    }

    uart_send_cmd(CMD_RF_STS_SYSC, 1, 0);

    /* Avoid double-sending right after a sync. */
    uart_rpt_timer = timer_read32();

    if (dev_info.link_mode != LINK_USB) {
        if (++sync_lost >= 5) {
            sync_lost  = 0;
            f_rf_reset = 1;
        }
    }
}

/* Single-frame UART send. Rate-limited to one frame per ms and stamps
 * the last-send timestamp; the receive path uses that to avoid clashing
 * with an in-flight TX (which is what caused the stock firmware to lose
 * sync and crash). */
void uart_send_bytes(uint8_t *Buffer, uint32_t Length) {
    Usart_Mgr.RXCmd = CMD_NULL;
    if (timer_elapsed32(Usart_Mgr.TXLastCmdTm) < 1) {
        wait_ms(1);
    }
    gpio_write_pin_low(NRF_WAKEUP_PIN);
    wait_us(50);
    uart_transmit(Buffer, Length);
    wait_us(50 + Length * 30);
    gpio_write_pin_high(NRF_WAKEUP_PIN);
    wait_us(200);
    Usart_Mgr.TXLastCmdTm = timer_read32();
}

uint8_t get_checksum(uint8_t *buf, uint8_t len) {
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < len; i++) checksum += *buf++;
    checksum ^= UART_HEAD;
    return checksum;
}

void uart_send_report(uint8_t report_type, uint8_t *report_buf, uint8_t report_size) {
    if (f_dial_sw_init_ok == 0) return;
    if (dev_info.link_mode == LINK_USB) return;
    if (dev_info.rf_state != RF_CONNECT) return;

    Usart_Mgr.TXDBuf[0] = UART_HEAD;
    Usart_Mgr.TXDBuf[1] = report_type;
    Usart_Mgr.TXDBuf[2] = 0x01;
    Usart_Mgr.TXDBuf[3] = report_size;

    memcpy(&Usart_Mgr.TXDBuf[4], report_buf, report_size);
    Usart_Mgr.TXDBuf[4 + report_size] = get_checksum(&Usart_Mgr.TXDBuf[4], report_size);

    uart_send_bytes(&Usart_Mgr.TXDBuf[0], report_size + 5);

    uart_rpt_timer = timer_read32();
}

void uart_receive_pro(void) {
    static bool     rcv_start = false;
    static uint32_t rcv_timer = 0;

    /* Don't overlap with our own TX, and don't busy-loop on RX. */
    if (timer_elapsed32(Usart_Mgr.TXLastCmdTm) < 1 || timer_elapsed32(rcv_timer) < 1) return;

    if (uart_available()) {
        /* Let the rest of the burst arrive before we start parsing. */
        wait_us(200);
        while (uart_available()) {
            uint8_t byte = uart_read();
            if (byte == UART_HEAD) {
                rcv_start = true;
            }
            if (rcv_start && Usart_Mgr.RXDLen < UART_MAX_LEN) {
                Usart_Mgr.RXDBuf[Usart_Mgr.RXDLen++] = byte;
            }
            /* Do not wait_us() inside this loop - the board crashes. */
        }

        if (rcv_start) {
            rcv_start          = false;
            Usart_Mgr.RXDState = RX_Done;
            rf_protocol_receive();
            Usart_Mgr.RXDLen = 0;
        }
    }
    rcv_timer = timer_read32();
}

/* NuPhy's QMK fork patches tmk_core/protocol/host.c to call into the
 * vendor's old uart_send_*_report() helpers (with `void` signatures)
 * before each report goes out. After the jincao1 port those side
 * effects all happen inside rf_driver.c / uart_send_report_repeat, so
 * the patched-in hooks become no-ops. Keep the symbols defined to
 * satisfy the linker without re-patching the upstream-shaped file. */
void uart_send_report_func(void) {}
void uart_send_mouse_report(void) {}
void uart_send_consumer_report(void) {}
void uart_send_system_report(void) {}

void rf_uart_init(void) {
    uart_init(460800);

    /* Enable parity check */
    USART1->CR1 &= ~((uint32_t)USART_CR1_UE);
    USART1->CR1 |= USART_CR1_M0 | USART_CR1_PCE;
    USART1->CR1 |= USART_CR1_UE;

    /* set Rx and Tx pin pull up */
    GPIOB->OSPEEDR &= ~(GPIO_OSPEEDER_OSPEEDR6 | GPIO_OSPEEDER_OSPEEDR7);
    GPIOB->PUPDR |= (GPIO_PUPDR_PUPDR6_0 | GPIO_PUPDR_PUPDR7_0);
}

void rf_device_init(void) {
    uint8_t timeout = 10;

    f_rf_hand_ok = 0;
    while (timeout--) {
        uart_send_cmd(CMD_HAND, 0, 20);
        wait_ms(5);
        uart_receive_pro();
        if (f_rf_hand_ok) break;
    }

    timeout           = 10;
    f_rf_read_data_ok = 0;
    while (timeout--) {
        uart_send_cmd(CMD_READ_DATA, 0, 20);
        wait_ms(5);
        uart_receive_pro();
        if (f_rf_read_data_ok) break;
    }

    timeout          = 10;
    f_rf_sts_sysc_ok = 0;
    while (timeout--) {
        uart_send_cmd(CMD_RF_STS_SYSC, 0, 20);
        wait_ms(5);
        uart_receive_pro();
        if (f_rf_sts_sysc_ok) break;
    }

    uart_send_cmd(CMD_SET_NAME, 10, 20);
}
