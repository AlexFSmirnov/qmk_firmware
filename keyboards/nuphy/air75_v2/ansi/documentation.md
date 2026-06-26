## Air75 V2 ANSI - Custom Changes

Reference summary of modifications made on top of upstream
`nuphy-src/qmk_firmware:nuphy-keyboards`. Only the `air75_v2/ansi` board is
affected (plus two small upstream RGB animation tweaks).

## File map

| File                                       | Purpose                                                   |
| ------------------------------------------ | --------------------------------------------------------- |
| `user.c` / `user.h`                        | `process_record_user`, hooks, vim, housekeeping driver    |
| `utils.c` / `utils.h`                      | LED helpers + `user_config` persistence helpers           |
| `layers.c` / `layers.h`                    | Per-layer overlay system (side color, white keys, custom) |
| `macros.c` / `macros.h`                    | Runtime-recordable macros (6 slots, EEPROM-persisted)     |
| `qmk-vim/`                                 | Vendored [andrewjrae/qmk-vim](https://github.com/andrewjrae/qmk-vim) |
| `keymaps/custom/`                          | Single source-of-truth keymap (VIA-enabled)               |
| `rf_queue.c` / `rf_queue.h`                | 64-slot circular buffer for HID reports while RF is reconnecting (ported from [jincao1/qmk_firmware](https://github.com/jincao1/qmk_firmware)) |
| `mcu_pwr.c` / `mcu_pwr.h` / `mcu_stm32f0xx.h` | STM32F072 deep/light-sleep + LED rail power gating (ported from jincao1) |
| `documentation.md`                         | This file                                                 |

Build with `qmk compile -kb nuphy/air75_v2/ansi -km custom`.

## Layer map

| Layer | Const                | Activator         | Role                                |
| ----- | -------------------- | ----------------- | ----------------------------------- |
| 0     | `MAC_BASE_LAYER`     | default           | Mac base                            |
| 1     | `MAC_FN_LAYER`       | Fn (Mac base)     | Mac Fn (links + F13..F24 + BAT_NUM) |
| 2     | `WIN_BASE_LAYER`     | default           | Win base                            |
| 3     | `WIN_FN_LAYER`       | Fn (Win base)     | Win Fn (same content as Mac Fn)     |
| 4     | `VIM_NAV_LAYER`      | Caps Lock or RAlt | hjkl-arrow navigation (both Mac+Win) |
| 5     | `CONFIG_LAYER`       | Fn + Del          | RGB matrix + side LED + system      |
| 6     | `MACRO_LAYER`        | Right Ctrl        | 6 macro slots (F7-F12: record/play/delete) |

Notable keymap changes (apply to both Mac and Win):

- **Caps Lock** is `MO(L_VIM)` on both Mac and Win base layers.
- **Right Alt position** (`RGui` on Mac, `RAlt` on Win) is also `MO(L_VIM)`
  so both hands can reach hjkl navigation.
- **Right Ctrl position** is now `MO(L_MCR)`. The keyboard loses RCtl; if
  you need it, remap a different key in VIA.
- **Right column** (the navigation cluster): `PgUp` -> `VIM_TOGGLE`,
  `PgDn` -> `F13`, `Home` -> `F14`, `End` -> `F15`. Vim toggle lives next
  to the home row for quick reach; F13-F15 are useful as binding targets
  in the OS (window managers, app shortcuts, etc.).
- **Fn + PgUp position** (the same key as the new `VIM_TOGGLE`) is
  `VIM_LOCK` - hard-disables vim mode until pressed again.
- **F-row**: both Mac and Win base layers have plain `KC_F1..F12` on the
  function row. Holding `Fn` swaps those to system / media controls:
  - Mac Fn: full set - `BRID, BRIU, MAC_TASK, MAC_SEARCH, MAC_VOICE,
    MAC_DND, MPRV, MPLY, MNXT, MUTE, VOLD, VOLU` (original NuPhy F-row).
  - Win Fn: subset - `BRID, BRIU, _, _, _, _, MPRV, MPLY, MNXT, MUTE,
    VOLD, VOLU` (F3..F6 left transparent because the Mac-specific
    keycodes for those positions don't map cleanly to Windows).
- Short layer aliases (`L_MAC`, `L_MFN`, `L_WIN`, `L_WFN`, `L_VIM`,
  `L_CFG`, `L_MCR`) are defined in `layers.h` / `macros.h` so `MO()`
  calls stay compact and the keymap columns can align cleanly.
- Empty positions on overlay layers use `_______` (= `KC_TRNS`, falls
  through) rather than `XXXXXXX` (= `KC_NO`, blocks). This means
  modifiers and base-layer keys still work while an overlay is held; the
  active-key whitening in `layers.c` only lights *non-transparent*
  positions, so falling through doesn't add stray highlights.
- **Fn layer top 3 rows**:
  - Row 0 (F-row): nothing.
  - Row 1 (numbers): `1`=LNK_BLE1, `2`=LNK_BLE2, `3`=LNK_BLE3 (all blue),
    `4`=LNK_RF (yellow). Everything else: nothing.
  - Row 2 (QWERTY): `Q..R`=F13..F16 (red), `T..I`=F17..F20 (green),
    `O..]`=F21..F24 (blue). `Tab`, `\`, `PgDn`: nothing.
- **Fn layer rows 3-5** are mostly transparent. `BAT_NUM` lives on the `B`
  key. Everything else (`DEV_RESET`, `SLEEP_MODE`, `BAT_SHOW`, all RGB/SIDE
  controls) moved to the Config layer.
- **Fn + Del** opens the Config layer (replacing the old `MO(5)` on the V key).

### Config layer (5) layout

Reached via **Fn + Del**. Color-coded in `layers.c`:

| Position    | Keycode       | Notes                       |
| ----------- | ------------- | --------------------------- |
| Q / A       | `RGB_MOD`/`RGB_RMOD` | matrix effect next/prev |
| W / S       | `RGB_VAI`/`RGB_VAD`  | matrix brightness ±    |
| E / D       | `RGB_HUI`/`RGB_HUD`  | matrix hue ±           |
| R / F       | `RGB_SPI`/`RGB_SPD`  | matrix speed ±         |
| U / J       | `SIDE_MOD`/`SIDE_RMOD` | side mode next/prev  |
| I / K       | `SIDE_VAI`/`SIDE_VAD`  | side brightness ±    |
| O / L       | `SIDE_HUI`/`SIDE_HUD`  | side color next/prev |
| P / `;`     | `SIDE_SPI`/`SIDE_SPD`  | side speed ±         |
| B           | `BAT_SHOW`    | green - battery indicator   |
| N           | `SLEEP_MODE`  | orange - auto-sleep toggle  |
| M           | `DEV_RESET`   | red - factory reset (long press) |

Matrix controls are cyan (forward=bright, backward=dim). Side controls are
magenta (forward=bright, backward=dim). New custom keycodes `SIDE_RMOD`
and `SIDE_HUD` are wired in `ansi.c`.

### Macro layer (6)

Reached by holding **Right Ctrl**. Columns **F7 through F12** (and the
aligned keys on the number and U rows) are six parallel slot columns:

| Row             | Action                                            |
| --------------- | ------------------------------------------------- |
| F7..F12         | Play slot 1..6 with the original recorded delays  |
| 7..=            | Play slot 1..6 instantly (no delays)              |
| U..]            | Record (if empty) / Save (if recording this slot) / Delete (if occupied) |
| Esc             | Cancel the current recording (no save)            |

Slot colors (all scaled by the RGB matrix brightness):

- **Empty slot**: only the U-row record key is lit, in white. F-row and
  number-row keys are **not overpainted** - the active RGB matrix effect
  (solid reactive, etc.) keeps running there.
- **Occupied slot**: F-row = yellow (play delayed), number row = green
  (play instant), U-row = red (erase).
- **Currently recording into this slot**: only the U-row key blinks red;
  the play-row keys are left to the matrix effect.
- **Currently playing this slot**: the corresponding play key (F-row for
  delayed, number row for instant) pulses brighter; other keys stay at
  their occupied-slot colors.
- **Esc**: bright red while a recording is in progress (acts as cancel).

Side LEDs:

- **Recording**: full-red blink on both sides, scaled by the **side LED
  brightness** (`SIDE_VAI` / `SIDE_VAD`) - same as every other side
  indicator, so the existing brightness controls work.
- **Playing**: no side LED override (so the matrix reactive effect is the
  only feedback). If the user is still holding Right Ctrl during playback,
  the macro-layer side color (orange) shows; otherwise the base-layer
  animation runs.

Recording flow:

1. Hold Right Ctrl, press a U-row key for an **empty** (white) slot. Side
   LEDs go red.
2. Release Right Ctrl and type normally. Up to **128 events** are captured
   per slot (press and release are separate events). Delays between events
   are recorded to the nearest 10ms. **Any key event that fires while
   the macro layer is active is NOT recorded** (including the Right Ctrl
   activator itself), so toggling the macro layer is safe.
3. Press Right Ctrl + the same U-row key to **save**, or Right Ctrl + Esc to
   **cancel**. The slot is auto-saved if it hits 128 events.

Playback uses `register_code16`/`unregister_code16` for each event, and
calls `rgb_matrix_handle_key_event` so reactive RGB effects (Solid
Reactive, Splash, etc.) light up the original physical keys as the macro
plays. Instant mode still leaves a 1ms gap between events so the OS
doesn't drop them.

Persistence: each slot occupies **516 bytes** in the user data block (4
byte header + 128 × 4-byte events). Six slots use **3112 bytes** total
(user_config + padding + slots). `EECONFIG_USER_DATA_SIZE` remains **3200**
in `config.h` (same as before — fewer slots offset the larger per-slot size).
`user_config_save()` / `user_config_load()` in `utils.c` do **partial**
writes covering only their own 8 bytes so they don't trample the macro
region.

Known limitations:

- Layer-switch keycodes (MO/TG/etc.) inside a macro will be recorded but
  may not replay the way you'd expect, since `register_code16` doesn't run
  layer logic. Stick to plain keys/modifiers for reliable macros.
- Any key event that fires while the macro layer is active is dropped from
  the recording (this also covers the macro-layer activator itself).

## Layer overlay system (`layers.c`)

On any non-base, non-macro layer the overlay system overrides RGB rendering:

1. **Active-key whitening** — every key whose effective keycode on the active
   layer is not `KC_NO` / `KC_TRNS` is lit white at the user's current RGB
   brightness. The lookup goes through `keymap_key_to_keycode`, so VIA edits
   are reflected automatically.
2. **Per-key color overrides** — entries in `layers.c`'s `*_keys[]` arrays
   replace the default white for those `(row, col)` positions. Stored in
   `PROGMEM`; RGB values are scaled by user brightness. Overrides are
   *only* applied to keys that have a non-transparent assignment on this
   layer, so a typo in the coords can't light an unrelated key.
3. **Per-layer side LED color** — both side strips are painted with the
   configured RGB, scaled by the **side LED brightness** (`SIDE_VAI` /
   `SIDE_VAD`) via `set_side_l_rgb_side` / `set_side_r_rgb_side` (see
   `utils.c`). The same SIDE brightness control that affects the stock
   animations therefore applies to layer overlays.

Edit the `USER-EDITABLE SECTION` block in `layers.c` to tune colors.
Helpers `SIDE_BOTH(r,g,b)` and `SIDE_SPLIT(lr,lg,lb,rr,rg,rb)` are
provided.

The macro layer is in `layer_overlays[]` only for its side color
(orange); its matrix keys are painted by
`macros.c::macro_render_indicators()`.

### VIA interoperability

- The custom keymap is the firmware default *and* the VIA factory layout.
- Active-key whitening reads through `keymap_key_to_keycode`, so VIA-edited
  keys participate correctly.
- **Not** configurable from VIA: per-key color overrides, per-layer side
  colors, macro slot contents (those are recorded directly on the keyboard).

## Utility helpers (`utils.c` / `utils.h`)

```c
uint8_t scale8(uint8_t value, uint8_t scale);

void set_key_rgb(uint8_t row, uint8_t col, uint8_t r, uint8_t g, uint8_t b);
void set_key_hsv(uint8_t row, uint8_t col, uint8_t h, uint8_t s, uint8_t v);

/* Scaled (>> 2 internally) - match stock side animation look */
void set_side_l_rgb(uint8_t r, uint8_t g, uint8_t b);
void set_side_r_rgb(uint8_t r, uint8_t g, uint8_t b);
void set_sides_rgb (uint8_t r, uint8_t g, uint8_t b);
void set_sides_hsv (uint8_t h, uint8_t s, uint8_t v);

/* Full-range - write directly to the side LED buffer */
void set_side_l_rgb_full(uint8_t r, uint8_t g, uint8_t b);
void set_side_r_rgb_full(uint8_t r, uint8_t g, uint8_t b);
void set_sides_rgb_full (uint8_t r, uint8_t g, uint8_t b);
void set_sides_hsv_full (uint8_t h, uint8_t s, uint8_t v);

/* Side-brightness aware - scale incoming RGB by the current SIDE LED
 * brightness level (SIDE_VAI / SIDE_VAD).  Use these for any custom
 * indicator that lives on the side strip so the existing brightness
 * controls work as expected. */
uint8_t side_brightness_scale(void); /* 0..255 (current side_light level) */
void set_side_l_rgb_side(uint8_t r, uint8_t g, uint8_t b);
void set_side_r_rgb_side(uint8_t r, uint8_t g, uint8_t b);
void set_sides_rgb_side (uint8_t r, uint8_t g, uint8_t b);
void set_sides_hsv_side (uint8_t h, uint8_t s, uint8_t v);

/* user_config persistence (partial write over the user data block) */
void user_config_load(void);
void user_config_save(void);
```

## Vim mode (`qmk-vim` integration)

Custom keycodes added in `ansi.h`:

- `VIM_HOLD` - enable vim mode while held.
- `VIM_TOGGLE` - toggle vim mode (no-op when locked-disabled).
- `VIM_LOCK` - hard-disable vim mode until pressed again (red indicator).

Behaviour in `user.c`:

- `process_record_user` first routes through `macro_process_record`, then
  through `process_vim_mode`.
- `process_insert_mode_user`:
  - Double-tap `j` within `VIM_DOUBLE_J_DELAY` (300 ms) sends `BSPC` and
    returns to normal mode.
  - `Ctrl+W` is rewritten to `Ctrl+Bspc`.
- `Ctrl+Cmd+H` / `Ctrl+Cmd+L` are remapped to `Ctrl+Cmd+Left` /
  `Ctrl+Cmd+Right` (macOS Spaces navigation), regardless of vim mode.
- The vim-mode key indicator (`[1][16]`) is drawn *after* the layer
  overlay, so it always wins.
- Side LED priority in `side_led_show_user`: macro > vim > layer overlay >
  stock animation.

`config.h` enables: `BETTER_VISUAL_MODE`, `VIM_G_MOTIONS`,
`VIM_PASTE_BEFORE`, `VIM_REPLACE`, `VIM_DOT_REPEAT`,
`VIM_W_BEGINNING_OF_WORD`, `VIM_NUMBERED_JUMPS`.

## RGB / lighting

- Default mode: `RGB_MATRIX_SOLID_REACTIVE`.
- `keyboard.json` `rgb_matrix.max_brightness` = `255` (raised from 128).
- `RGB_MATRIX_HUE_STEP` = `4` (halved from the default 8) so `RGB_HUI` /
  `RGB_HUD` give twice the precision per press.
- `quantum/rgb_matrix/animations/solid_reactive_anim.h`: reactive effect
  desaturates toward the base colour on press instead of hue-shifting.
- `quantum/rgb_matrix/animations/typing_heatmap_anim.h`: adds a
  commented-out "fade from white" alternative; behaviour unchanged.
- `side.c`: side LED update loop calls `side_led_show_user()` first.
- `side_light_table` expanded from 6 to **11 levels** (intermediate
  midpoints added between the original `{0, 22, 34, 55, 79, 106}`
  values), and `SIDE_BRIGHT_MAX` raised to 9, so `SIDE_VAI` / `SIDE_VAD`
  give 2x finer brightness control while preserving the max brightness
  of the original.
- `colour_lib` expanded from 8 to **16 colors** (midpoints interpolated
  between the original 8 rainbow stops; the wrap stop is a pink-rose),
  and `SIDE_COLOUR_MAX` raised to 16, so `SIDE_HUI` / `SIDE_HUD` give 2x
  finer hue control.
- Custom indicators (layer overlay sides, macro recording, vim) use the
  `_side` helpers in `utils.c`, which scale by `side_light_table`, so
  the side-LED brightness controls apply uniformly to stock and custom
  indicators alike.

## Timing / sleep

- `no_act_time` widened from `uint16_t` to `uint32_t` (in `ansi.c`, `ansi.h`,
  `rf.c`, `sleep.c`).
- `LINK_TIMEOUT` reduced to 2 minutes; `SLEEP_TIME_DELAY` extended to 1 hour.
- `process_record_kb` resets `no_act_time` *before* delegating to
  `process_record_user` (so user-handled keys still count as activity).
- See **RF stability + power port** below for the deep-sleep + LED rail
  gating logic.

## RF stability + power port (from jincao1/qmk_firmware)

Most of this section is a selective port of the bug-fix half of
[jincao1/qmk_firmware](https://github.com/jincao1/qmk_firmware) (his
`air75v2-sleep` branch). All keymap and indicator customizations from
that fork were intentionally **skipped**; only the reliability and
battery-life work was brought over, and adapted to our `user_config_t`
and `user.c` layout. Files touched: `rf.c`, `rf_driver.c`, `ansi.{c,h}`,
`sleep.c`, `side.c`, `side_table.h`, `rules.mk`, `keyboard.json`, and
the four new files listed in the file map.

### Lost keys on wake / reconnect — `rf_queue.{c,h}`

- 64-slot circular buffer of `report_buffer_t` (`cmd` + 32-byte payload
  + length). `rf_driver.c::send_or_queue()` decides per-report whether
  to push straight onto UART or queue. While the RF link is down or the
  receiver hasn't ACKed, every HID report (keyboard, NKRO, mouse,
  extras) is enqueued instead of dropped.
- `uart_send_repeat_from_queue()` (in `rf.c`) drains the queue once the
  link comes back, in order, so a wake-up burst of keystrokes replays
  cleanly instead of being lost.
- `clear_report_buffer_and_queue()` is called from `break_all_key()` so
  a link switch (USB ↔ RF ↔ BT) doesn't keep replaying a stale held-key
  report after the switch.
- Buffer cost: ~1.2 KB of RAM. This is why `side_table.h` was trimmed
  (see below) — the F072 only has 16 KB of SRAM.

### Retransmission + UART race fix — `rf.c`

- Last keyboard / NKRO report is cached in `report_buff_a` /
  `report_buff_b` and re-sent on an adaptive interval
  (`get_repeat_interval()`) until activity stops, hardening against the
  RF dongle dropping a single packet (the most common cause of stuck or
  lost keys).
- `uart_send_bytes()` rate-limits to ≤1 command/ms and stamps
  `Usart_Mgr.TXLastCmdTm`. `uart_receive_pro()` waits for the full RX
  burst and ignores frames that arrive while we're mid-TX, so the
  MCU↔RF UART can't desync (the underlying cause of the random
  wireless freezes).
- `rf_protocol_receive()` validates `RX_LEN` and checksum and drops bad
  frames instead of feeding them into the state machine.
- `dev_sts_sync()` no longer issues `CMD_SET_24G_NAME` on every sync
  pass (it ran on a 50 ms timer and was a steady source of UART
  contention).

### Sleep + LED power gating — `mcu_pwr.{c,h}`, `mcu_stm32f0xx.h`, `sleep.c`

- `enter_deep_sleep()` puts the STM32F0 into `PWR_EnterSTOPMode` after
  cutting power to the LED rails (`pwr_rgb_led_off` /
  `pwr_side_led_off`). EXTI on the matrix wakes it back up.
- `enter_light_sleep()` only kills the LED rails (MCU stays awake) and
  is used in two cases where deep sleep is unsafe:
  - **USB connected** — wakeup from STOP mode tends to upset the USB
    PHY; we'd rather burn a few mA than crash the link.
  - **Wirelessly charging** — the charge controller's wake pulses
    would otherwise bounce us in and out of STOP repeatedly.
- `rgb_led_last_act` / `side_led_last_act` activity counters are bumped
  every ms in `timer_pro()` and reset in `pre_process_record_kb()`. The
  `led_power_handle()` call in `rgb_matrix_indicators_kb` cuts the LED
  driver when idle, regardless of whether the MCU is sleeping. This is
  the change responsible for most of the battery-life improvement on
  the jincao1 fork.
- `pre_process_record_kb()` runs the wake-up handoff (`f_wakeup_prepare`
  → `exit_light_sleep`) *before* QMK does anything else with the
  keystroke, so the first key after wake registers normally.

### Other touched files

- `rf_driver.c`: routes through `send_or_queue()`, force-sets
  `keyboard_protocol = 1` for NKRO before each report so the host
  doesn't snap back to boot protocol after a reconnect.
- `ansi.c`:
  - `gpio_init()` and `device_reset_show()` use the new
    `pwr_rgb_led_on()` / `pwr_side_led_on()` helpers so the activity
    counters and rail state stay in sync with reality.
  - `housekeeping_task_kb()` calls `uart_send_report_repeat()` instead
    of the old one-shot `uart_send_report_func()`.
  - Vendor-patched `uart_send_*_report()` hooks in `tmk_core/protocol/host.c`
    are stubbed as no-ops (the new flow already does the right thing
    inside `rf_driver.c`).
- `side.c`: `flush_side_leds` flag + `side_leds_all_zero()` skip the
  DMA push when nothing changed; refresh interval tightened from 30 ms
  to 10 ms for smoother indicator animations now that the push is
  cheap; `device_reset_show()` / `rgb_test_show()` go through the
  power helpers.
- `side_table.h`:
  - dropped `light_value_tab` entirely (−256 B);
  - shrank `breathe_data_tab` 256 → 128 entries (−128 B) and
    `wave_data_tab` 256 → 112 entries (−144 B) to match actual usage;
  - reduced `FLOW_COLOUR_TAB_LEN` 512 → 224.
  - Combined, this just about pays for the RF queue.
- `keyboard.json`: `debounce` 2 → 3 (a known stability win on the
  V2 matrix); `rgb_matrix.sleep = true` for the new sleep path.
- `rules.mk`: adds `mcu_pwr.c` and `rf_queue.c` to `SRC`, and enables
  `LTO_ENABLE = yes` (drops ~5 KB of flash, which we need to fit the
  RF queue + retransmission paths).

### What was intentionally NOT ported from jincao1

- All keymap and `process_record_*` keymap-side behaviour (we keep our
  own keymap, vim, macros, layer overlays).
- The custom battery / sleep-mode indicators on F-row, number row, and
  side LEDs.
- The split `kb_config_t` / `user_kb.c` layout — our `user_config_t` /
  `user.c` setup is kept and `mcu_pwr.c` was adapted to use it.

## Build

`rules.mk`:

```
include $(KEYBOARD_PATH_1)/qmk-vim/rules.mk
SRC += side.c rf.c sleep.c side_driver.c rf_driver.c \
       user.c utils.c layers.c macros.c \
       mcu_pwr.c rf_queue.c
LTO_ENABLE = yes
```

`keymaps/custom/rules.mk` enables VIA:

```
VIA_ENABLE = yes
```

## EEPROM layout

`config.h`:

```c
#define FEE_DENSITY_BYTES           6144   /* up from default 4096 */
#define EECONFIG_USER_DATA_SIZE     3200
#define EECONFIG_USER_DATA_VERSION  0x1A75C003
#define DYNAMIC_KEYMAP_LAYER_COUNT  7
```

STM32F072xB has 8 KB of flash reserved for emulated EEPROM. The default
split is 4 KB usable + 4 KB write log; we shift that to 6 KB usable + 2
KB write log to fit both the macro storage and VIA's dynamic keymap.
`DYNAMIC_KEYMAP_LAYER_COUNT` is dropped from 8 to 7 because that's all
the custom keymap defines, saving 192 bytes of EEPROM for VIA.

Within the user data block:

| Offset      | Size | Contents                                  |
| ----------- | ---- | ----------------------------------------- |
| 0..7        | 8    | `user_config_t` (side mode, sleep, etc.)  |
| 8..15       | 8    | padding                                   |
| 16..531     | 516  | macro slot 0 (F7 column)                  |
| 532..1047   | 516  | macro slot 1 (F8 column)                  |
| ...         | ...  | ...                                       |
| 2588..3103  | 516  | macro slot 5 (F12 column)                 |
| 3104..3199  | 96   | headroom                                  |

Total EEPROM map (6144 bytes):

| Region             | Bytes | Notes                                  |
| ------------------ | ----- | -------------------------------------- |
| EECONFIG base      | ~36   | layer state, RGB matrix config, etc.   |
| User data block    | 3200  | user_config + 6 × 516-byte macro slots |
| Dynamic keymap     | 1344  | 7 layers × 6 rows × 16 cols × 2 bytes  |
| VIA dynamic macros | ~1570 | leftover (unused; we have our own)     |

Bumping `EECONFIG_USER_DATA_VERSION` invalidates the block on next boot,
forcing `device_reset_init()` to write defaults (existing macro slots are
also wiped).

## Misc

- `keymaps/custom/NuPhy Air75 V2 via3.json` — VIA layout file. `customKeycodes`
  lists all `QK_KB_n` entries from `ansi.h` *in order*. New keycodes
  (`SIDE_RMOD`, `SIDE_HUD`) are appended at the end of the enum (NOT
  inserted in the middle) so existing VIA mappings stay stable.
- `keyboard.json` deviates from upstream in three spots: VIA layout
  + the `debounce` / `rgb_matrix.sleep` changes covered in the RF +
  power port section.
- `rf.c` has been substantially rewritten (queue + retransmission +
  UART race fix). See the RF + power port section for the full story.
