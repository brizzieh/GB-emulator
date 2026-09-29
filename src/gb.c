#include "gb_internal.h"

#include <string.h>

void gb_init(GameBoy *gb)
{
    memset(gb, 0, sizeof(*gb));
    gb->a = 0x01;
    gb->f = 0xB0;
    gb->b = 0x00;
    gb->c = 0x13;
    gb->d = 0x00;
    gb->e = 0xD8;
    gb->h = 0x01;
    gb->l = 0x4D;
    gb->sp = 0xFFFE;
    gb->pc = 0x0100;
    gb->mbc1_rom_low = 1;
    gb->memory[0xFF00] = 0x30;
    gb->memory[0xFF04] = 0xAB;
    gb->memory[0xFF0F] = 0xE1;
    gb->memory[0xFF40] = 0x91;
    gb->memory[0xFF41] = 0x85;
    gb->memory[0xFF47] = 0xFC;
    gb->memory[0xFF48] = 0xFF;
    gb->memory[0xFF49] = 0xFF;
    for (size_t pixel_index = 0; pixel_index < GB_SCREEN_PIXELS; ++pixel_index) {
        gb->framebuffer[pixel_index] = 0xFFFFFFFFu;
    }
}

void gb_advance(GameBoy *gb, unsigned cycles)
{
    for (unsigned cycle_index = 0; cycle_index < cycles; ++cycle_index) {
        gb_timer_tick(gb);
        gb_ppu_tick(gb);
    }
    gb->cycles += cycles;
}

void gb_run_cycles(GameBoy *gb, uint64_t cycles)
{
    uint64_t target_cycles = gb->cycles + cycles;
    while (gb->cycles < target_cycles && !gb->illegal_opcode) {
        gb_step(gb);
    }
}

uint32_t gb_frame_hash(const GameBoy *gb)
{
    uint32_t hash = 2166136261u;
    for (size_t pixel_index = 0; pixel_index < GB_SCREEN_PIXELS; ++pixel_index) {
        hash ^= gb->framebuffer[pixel_index];
        hash *= 16777619u;
    }
    return hash;
}
