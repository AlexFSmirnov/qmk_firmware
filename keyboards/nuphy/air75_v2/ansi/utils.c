#include "ansi.h"

uint8_t scale8(uint8_t value, uint8_t scale) {
    return ((uint16_t)value * (uint16_t)scale) >> 8;
}
