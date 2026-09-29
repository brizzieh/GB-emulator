#include "gb_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t rom_bank_count(const GameBoy *gb)
{
    return gb->rom_size / 0x4000u;
}

static int has_mbc1(const GameBoy *gb)
{
    return gb->cartridge_type >= 0x01 && gb->cartridge_type <= 0x03;
}

static size_t rom_bank_for_address(const GameBoy *gb, uint16_t address)
{
    size_t bank = 0;
    if (address < 0x4000) {
        if (has_mbc1(gb) && gb->mbc1_mode) {
            bank = (size_t)(gb->mbc1_bank_high & 3) << 5;
        }
    } else if (has_mbc1(gb)) {
        unsigned low_bank = gb->mbc1_rom_low & 0x1F;
        if (low_bank == 0) low_bank = 1;
        bank = ((size_t)(gb->mbc1_bank_high & 3) << 5) | low_bank;
    } else {
        bank = 1;
    }

    size_t count = rom_bank_count(gb);
    return count ? bank % count : 0;
}

uint8_t gb_cartridge_read(GameBoy *gb, uint16_t address)
{
    if (address < 0x8000) {
        if (!gb->rom) return 0xFF;
        size_t bank = rom_bank_for_address(gb, address);
        size_t offset = bank * 0x4000u + (address & 0x3FFFu);
        return offset < gb->rom_size ? gb->rom[offset] : 0xFF;
    }

    if (address >= 0xA000 && address < 0xC000) {
        if (!gb->cart_ram || !gb->mbc1_ram_enabled) return 0xFF;
        size_t bank = has_mbc1(gb) && gb->mbc1_mode
            ? (gb->mbc1_bank_high & 3u) : 0u;
        size_t offset = bank * 0x2000u + (address - 0xA000u);
        if (gb->cart_ram_size == 0x800u) offset &= 0x7FFu;
        return offset < gb->cart_ram_size ? gb->cart_ram[offset] : 0xFF;
    }
    return 0xFF;
}

void gb_cartridge_write(GameBoy *gb, uint16_t address, uint8_t value)
{
    if (address < 0x8000) {
        if (!has_mbc1(gb)) return;
        if (address < 0x2000) {
            gb->mbc1_ram_enabled = (value & 0x0F) == 0x0A;
        } else if (address < 0x4000) {
            gb->mbc1_rom_low = value & 0x1F;
            if (gb->mbc1_rom_low == 0) gb->mbc1_rom_low = 1;
        } else if (address < 0x6000) {
            gb->mbc1_bank_high = value & 3;
        } else {
            gb->mbc1_mode = value & 1;
        }
        return;
    }

    if (address >= 0xA000 && address < 0xC000 &&
        gb->cart_ram && gb->mbc1_ram_enabled) {
        size_t bank = has_mbc1(gb) && gb->mbc1_mode
            ? (gb->mbc1_bank_high & 3u) : 0u;
        size_t offset = bank * 0x2000u + (address - 0xA000u);
        if (gb->cart_ram_size == 0x800u) offset &= 0x7FFu;
        if (offset < gb->cart_ram_size) gb->cart_ram[offset] = value;
    }
}

static size_t ram_size_from_header(uint8_t code)
{
    switch (code) {
        case 1: return 0x800;
        case 2: return 0x2000;
        case 3: return 0x8000;
        case 4: return 0x20000;
        case 5: return 0x10000;
        default: return 0;
    }
}

int gb_load_rom(GameBoy *gb, const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file) {
        perror(path);
        return 0;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }
    long length = ftell(file);
    if (length < 0x150 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        fprintf(stderr, "ROM is too small or could not be read.\n");
        return 0;
    }

    uint8_t *rom = (uint8_t *)malloc((size_t)length);
    if (!rom) {
        fclose(file);
        fprintf(stderr, "Not enough memory to load ROM.\n");
        return 0;
    }
    size_t read_count = fread(rom, 1, (size_t)length, file);
    fclose(file);
    if (read_count != (size_t)length) {
        free(rom);
        fprintf(stderr, "Could not read the complete ROM.\n");
        return 0;
    }

    free(gb->rom);
    free(gb->cart_ram);
    gb->rom = rom;
    gb->rom_size = read_count;
    gb->cartridge_type = rom[0x147];
    gb->cart_ram_size = gb->cartridge_type == 0x02 || gb->cartridge_type == 0x03
        ? ram_size_from_header(rom[0x149]) : 0;
    gb->cart_ram = gb->cart_ram_size
        ? (uint8_t *)calloc(gb->cart_ram_size, 1) : NULL;
    if (gb->cart_ram_size && !gb->cart_ram) {
        free(gb->rom);
        gb->rom = NULL;
        gb->rom_size = 0;
        gb->cart_ram_size = 0;
        fprintf(stderr, "Not enough memory to allocate cartridge RAM.\n");
        return 0;
    }
    gb->mbc1_rom_low = 1;
    gb->mbc1_bank_high = 0;
    gb->mbc1_mode = 0;
    gb->mbc1_ram_enabled = 0;

    char title[17];
    memcpy(title, &rom[0x134], 16);
    title[16] = '\0';
    printf("ROM loaded: %zu bytes\nTitle: %.16s\nCartridge type: %02X\nROM size code: %02X\n",
        gb->rom_size, title, gb->cartridge_type, rom[0x148]);
    return 1;
}

void gb_destroy(GameBoy *gb)
{
    free(gb->rom);
    free(gb->cart_ram);
    gb->rom = NULL;
    gb->cart_ram = NULL;
    gb->rom_size = 0;
    gb->cart_ram_size = 0;
}
