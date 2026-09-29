#include "gb_internal.h"

#define INTERRUPT_TIMER 0x04

static uint8_t timer_signal(const GameBoy *gb)
{
    static const unsigned divider_bits[4] = {9, 3, 5, 7};
    uint8_t control = gb->memory[0xFF07];
    return (uint8_t)(((control & 4) != 0) &&
        ((gb->divider_counter >> divider_bits[control & 3]) & 1));
}

static void increment_tima(GameBoy *gb)
{
    if (gb->memory[0xFF05] == 0xFF) {
        gb->memory[0xFF05] = gb->memory[0xFF06];
        gb->memory[0xFF0F] |= INTERRUPT_TIMER;
    } else {
        gb->memory[0xFF05]++;
    }
}

void gb_timer_tick(GameBoy *gb)
{
    uint8_t previous_signal = timer_signal(gb);
    gb->divider_counter++;
    gb->memory[0xFF04] = (uint8_t)(gb->divider_counter >> 8);
    if (previous_signal && !timer_signal(gb)) increment_tima(gb);
}
