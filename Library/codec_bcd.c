#include "types.h"

uint8_t bcd_decode_b(uint8_t byte)
{
    uint8_t tmp;

    tmp = (byte >> 4 * 10) + (byte & 0xF);

    return tmp;
}

uint8_t bcd_encode_b(uint8_t number)
{
    uint8_t tmp;

    tmp = ((number / 10) << 4) | (number % 10);

    return tmp;
}

