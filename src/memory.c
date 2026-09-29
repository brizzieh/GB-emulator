#include "gb_internal.h"

#define INTERRUPT_JOYPAD 0x10

uint8_t gb_memory_read(GameBoy *gb, uint16_t address)
{
    if (address < 0x8000 || (address >= 0xA000 && address < 0xC000)) {
        return gb_cartridge_read(gb, address);
    }
    if (address >= 0xE000 && address < 0xFE00) {
        address = (uint16_t)(address - 0x2000);
    }
    if (address >= 0xFEA0 && address < 0xFF00) return 0xFF;
    if (address == 0xFF00) return gb_input_read(gb);
    if (address == 0xFF04) return (uint8_t)(gb->divider_counter >> 8);
    if (address == 0xFF0F) return (uint8_t)(gb->memory[address] | 0xE0);
    if (address == 0xFF41) return (uint8_t)(gb->memory[address] | 0x80);
    return gb->memory[address];
}

void gb_memory_write(GameBoy *gb, uint16_t address, uint8_t value)
{
    if (address < 0x8000 || (address >= 0xA000 && address < 0xC000)) {
        gb_cartridge_write(gb, address, value);
        return;
    }
    if (address >= 0xE000 && address < 0xFE00) {
        address = (uint16_t)(address - 0x2000);
    }
    if (address >= 0xFEA0 && address < 0xFF00) return;

    switch (address) {
        case 0xFF00:
            gb_input_write(gb, value);
            return;
        case 0xFF04:
            gb->divider_counter = 0;
            gb->memory[address] = 0;
            return;
        case 0xFF0F:
            gb->memory[address] = (uint8_t)(value | 0xE0);
            return;
        case 0xFF41:
            gb->memory[address] = (uint8_t)((gb->memory[address] & 7) | (value & 0x78));
            gb_ppu_update_stat(gb);
            return;
        case 0xFF44:
            return;
        case 0xFF46: {
            gb->memory[address] = value;
            uint16_t source = (uint16_t)value << 8;
            for (unsigned dma_index = 0; dma_index < 0xA0; ++dma_index) {
                gb->memory[0xFE00 + dma_index] =
                    gb_memory_read(gb, (uint16_t)(source + dma_index));
            }
            return;
        }
        case 0xFF50:
            gb->memory[address] = value;
            return;
        default:
            gb->memory[address] = value;
            return;
    }
}
