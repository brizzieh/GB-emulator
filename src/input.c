#include "gb_internal.h"

#define INTERRUPT_JOYPAD 0x10

uint8_t gb_input_read(const GameBoy *gb)
{
    uint8_t select = gb->memory[0xFF00] & 0x30;
    uint8_t low = 0x0F;

    if (!(select & 0x10)) {
        for (unsigned key = 0; key < 4; ++key) {
            if (gb->keys[key]) low &= (uint8_t)~(1u << key);
        }
    }
    if (!(select & 0x20)) {
        for (unsigned key = 0; key < 4; ++key) {
            if (gb->keys[key + 4]) low &= (uint8_t)~(1u << key);
        }
    }
    return (uint8_t)(0xC0 | select | low);
}

void gb_input_write(GameBoy *gb, uint8_t value)
{
    uint8_t old_low = gb_input_read(gb) & 0x0F;
    gb->memory[0xFF00] = value & 0x30;
    if ((old_low & (uint8_t)~gb_input_read(gb)) & 0x0F) {
        gb->memory[0xFF0F] |= INTERRUPT_JOYPAD;
    }
}

void gb_set_key(GameBoy *gb, unsigned key, int pressed)
{
    if (key >= 8) return;
    uint8_t old_value = gb_input_read(gb);
    gb->keys[key] = pressed ? 1 : 0;
    uint8_t new_value = gb_input_read(gb);
    if ((old_value & (uint8_t)~new_value) & 0x0F) {
        gb->memory[0xFF0F] |= INTERRUPT_JOYPAD;
        gb->stopped = 0;
    }
}
