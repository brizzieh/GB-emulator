#ifndef GB_INTERNAL_H
#define GB_INTERNAL_H

#include "gb.h"

uint8_t gb_memory_read(GameBoy *gb, uint16_t address);
void gb_memory_write(GameBoy *gb, uint16_t address, uint8_t value);
uint8_t gb_cartridge_read(GameBoy *gb, uint16_t address);
void gb_cartridge_write(GameBoy *gb, uint16_t address, uint8_t value);
uint8_t gb_input_read(const GameBoy *gb);
void gb_input_write(GameBoy *gb, uint8_t value);
void gb_timer_tick(GameBoy *gb);
void gb_ppu_tick(GameBoy *gb);
void gb_ppu_update_stat(GameBoy *gb);
void gb_advance(GameBoy *gb, unsigned cycles);

#endif
