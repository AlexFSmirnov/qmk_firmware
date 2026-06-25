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
| `macros.c` / `macros.h`                    | Runtime-recordable macros (12 slots, EEPROM-persisted)    |
| `qmk-vim/`                                 | Vendored [andrewjrae/qmk-vim](https://github.com/andrewjrae/qmk-vim) |
| `keymaps/custom/`                          | Single source-of-truth keymap (VIA-enabled)               |
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
| 6     | `MACRO_LAYER`        | Right Ctrl        | 12 macro slots (record/play/delete) |

Notable keymap changes (apply to both Mac and Win):

- **Caps Lock** is `MO(L_VIM)` on both Mac and Win base layers.
- **Right Alt position** (`RGui` on Mac, `RAlt` on Win) is also `MO(L_VIM)`
  so both hands can reach hjkl navigation.
- **Right Ctrl position** is now `MO(L_MCR)`. The keyboard loses RCtl; if
  you need it, remap a different key in VIA.
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

Reached by holding **Right Ctrl**. Top 3 rows are 12 parallel slot columns:

| Row             | Action                                            |
| --------------- | ------------------------------------------------- |
| F1..F12         | Play slot 1..12 with the original recorded delays |
| 1..=            | Play slot 1..12 instantly (no delays)             |
| Q..]            | Record (if empty) / Save (if recording this slot) / Delete (if occupied) |
| Esc             | Cancel the current recording (no save)            |

Slot colors (all scaled by the RGB matrix brightness):

- **Empty slot**: only the Q-row record key is lit, in white. F-row and
  number-row keys are **not overpainted** - the active RGB matrix effect
  (solid reactive, etc.) keeps running there.
- **Occupied slot**: F-row = yellow (play delayed), number row = green
  (play instant), Q-row = red (erase).
- **Currently recording into this slot**: only the Q-row key blinks red;
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

1. Hold Right Ctrl, press a Q-row key for an **empty** (white) slot. Side
   LEDs go red.
2. Release Right Ctrl and type normally. Up to **64 events** are captured
   per slot (press and release are separate events). Delays between events
   are recorded to the nearest 10ms. **Any key event that fires while
   the macro layer is active is NOT recorded** (including the Right Ctrl
   activator itself), so toggling the macro layer is safe.
3. Press Right Ctrl + the same Q-row key to **save**, or Right Ctrl + Esc to
   **cancel**. The slot is auto-saved if it hits 64 events.

Playback uses `register_code16`/`unregister_code16` for each event, and
calls `rgb_matrix_handle_key_event` so reactive RGB effects (Solid
Reactive, Splash, etc.) light up the original physical keys as the macro
plays. Instant mode still leaves a 1ms gap between events so the OS
doesn't drop them.

Persistence: each slot occupies **260 bytes** in the user data block (4
byte header + 64 × 4-byte events). `EECONFIG_USER_DATA_SIZE` is set to
**3200** in `config.h` to fit user_config + 12 slots + headroom. Because
this exceeds the default 4 KB emulated EEPROM, `config.h` also bumps
`FEE_DENSITY_BYTES` to **6144** (2 KB write log remaining).
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

## Build

`rules.mk`:

```
include $(KEYBOARD_PATH_1)/qmk-vim/rules.mk
SRC += side.c rf.c sleep.c side_driver.c rf_driver.c user.c utils.c layers.c macros.c
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
#define EECONFIG_USER_DATA_VERSION  0x1A75C002
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
| 16..275     | 260  | macro slot 0                              |
| 276..535    | 260  | macro slot 1                              |
| ...         | ...  | ...                                       |
| 2876..3135  | 260  | macro slot 11                             |
| 3136..3199  | 64   | headroom                                  |

Total EEPROM map (6144 bytes):

| Region             | Bytes | Notes                                  |
| ------------------ | ----- | -------------------------------------- |
| EECONFIG base      | ~30   | layer state, RGB matrix config, etc.   |
| User data block    | 3200  | user_config + 12 × 260-byte macro slots |
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
- `keyboard.json` is otherwise unchanged from upstream besides reformatting.
- `rf.c` only has whitespace cleanup beyond the `no_act_time` type change.
