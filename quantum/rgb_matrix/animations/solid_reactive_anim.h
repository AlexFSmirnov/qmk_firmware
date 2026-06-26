#ifdef RGB_MATRIX_KEYREACTIVE_ENABLED
#    ifdef ENABLE_RGB_MATRIX_SOLID_REACTIVE
RGB_MATRIX_EFFECT(SOLID_REACTIVE)
#        ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

static HSV SOLID_REACTIVE_math(HSV hsv, uint16_t offset) {
#            ifdef RGB_MATRIX_SOLID_REACTIVE_GRADIENT_MODE
    hsv.h = scale16by8(g_rgb_timer, qadd8(rgb_matrix_config.speed, 8) >> 4);
#            endif
    /* Rest: full configured H/S/V. On press: snap to white (S=0), then
     * animate saturation back up to the rest colour as offset increases. */
    uint8_t off = (offset > 255) ? 255 : (uint8_t)offset;
    hsv.s         = scale8(off, hsv.s);
    return hsv;
}

bool SOLID_REACTIVE(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    uint16_t max_tick = 65535 / qadd8(rgb_matrix_config.speed, 1);
    uint8_t  val      = rgb_matrix_config.hsv.v;

    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        uint16_t tick = max_tick;
        for (int8_t j = g_last_hit_tracker.count - 1; j >= 0; j--) {
            if (g_last_hit_tracker.index[j] == i && g_last_hit_tracker.tick[j] < tick) {
                tick = g_last_hit_tracker.tick[j];
                break;
            }
        }

        uint16_t offset = scale16by8(tick, qadd8(rgb_matrix_config.speed, 1));
        HSV      hsv    = SOLID_REACTIVE_math(rgb_matrix_config.hsv, offset);
        /* Convert at full V, then scale linearly — same as layer overlays
         * (scale8 per channel). rgb_matrix_hsv_to_rgb applies the CIE1931
         * curve to V and makes base keys vanish several steps before overlays. */
        hsv.v           = UINT8_MAX;
        RGB rgb         = hsv_to_rgb_nocie(hsv);
        rgb.r           = scale8(rgb.r, val);
        rgb.g           = scale8(rgb.g, val);
        rgb.b           = scale8(rgb.b, val);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

#        endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
#    endif     // ENABLE_RGB_MATRIX_SOLID_REACTIVE
#endif         // RGB_MATRIX_KEYREACTIVE_ENABLED
