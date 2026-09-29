#ifndef GB_H
#define GB_H

#include <stddef.h>
#include <stdint.h>

#define GB_SCREEN_WIDTH 160
#define GB_SCREEN_HEIGHT 144
#define GB_SCREEN_PIXELS (GB_SCREEN_WIDTH * GB_SCREEN_HEIGHT)

typedef struct GameBoy
{
    uint8_t a, f, b, c, d, e, h, l;
    uint16_t sp, pc;

    uint8_t memory[0x10000];
    uint8_t *rom;
    size_t rom_size;
    uint8_t *cart_ram;
    size_t cart_ram_size;
    uint8_t cartridge_type;
    uint8_t mbc1_ram_enabled;
    uint8_t mbc1_rom_low;
    uint8_t mbc1_bank_high;
    uint8_t mbc1_mode;

    uint8_t ime;
    uint8_t ime_delay;
    uint8_t halted;
    uint8_t stopped;
    uint8_t illegal_opcode;
    uint8_t halt_bug;

    uint8_t keys[8];
    uint16_t divider_counter;
    uint16_t ppu_dot;
    uint8_t stat_irq_line;
    uint64_t cycles;
    uint64_t instructions;
    uint64_t frames;

    uint32_t framebuffer[GB_SCREEN_PIXELS];
} GameBoy;

void gb_init(GameBoy *gb);
int gb_load_rom(GameBoy *gb, const char *path);
void gb_destroy(GameBoy *gb);
int gb_step(GameBoy *gb);
void gb_run_cycles(GameBoy *gb, uint64_t cycles);
void gb_set_key(GameBoy *gb, unsigned key, int pressed);
uint32_t gb_frame_hash(const GameBoy *gb);

#endif
