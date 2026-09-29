#include "gb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned failures;

static void check(int passed, const char *name)
{
    if (!passed) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    } else {
        printf("PASS: %s\n", name);
    }
}

static int make_test_rom(GameBoy *gb, size_t banks)
{
    size_t size = banks * 0x4000u;
    gb_init(gb);
    gb->rom = (uint8_t *)calloc(size, 1);
    if (!gb->rom) return 0;
    gb->rom_size = size;
    gb->cartridge_type = 0x01;
    gb->mbc1_rom_low = 1;
    return 1;
}

static void test_alu_flags(void)
{
    GameBoy gb;
    if (!make_test_rom(&gb, 2)) {
        check(0, "allocate ALU test ROM");
        return;
    }
    const uint8_t program[] = {0x3E, 0x0F, 0xC6, 0x01, 0xFE, 0x10};
    memcpy(&gb.rom[0x100], program, sizeof(program));

    gb_step(&gb);
    gb_step(&gb);
    check(gb.a == 0x10 && gb.f == 0x20, "ADD immediate sets half-carry");
    gb_step(&gb);
    check(gb.a == 0x10 && gb.f == 0xC0, "CP immediate preserves A and sets zero/subtract");
    gb_destroy(&gb);
}

static void test_mbc1_bank_switch(void)
{
    GameBoy gb;
    if (!make_test_rom(&gb, 4)) {
        check(0, "allocate MBC1 test ROM");
        return;
    }
    const uint8_t program[] = {0x3E, 0x02, 0xEA, 0x00, 0x20, 0xC3, 0x00, 0x40};
    memcpy(&gb.rom[0x100], program, sizeof(program));
    gb.rom[0x8000] = 0x3E;
    gb.rom[0x8001] = 0x42;

    gb_step(&gb);
    gb_step(&gb);
    gb_step(&gb);
    gb_step(&gb);
    check(gb.a == 0x42, "MBC1 switches the 4000-7FFF ROM window");
    gb_destroy(&gb);
}

static void test_timer_overflow(void)
{
    GameBoy gb;
    if (!make_test_rom(&gb, 2)) {
        check(0, "allocate timer test ROM");
        return;
    }
    gb.memory[0xFF05] = 0xFF;
    gb.memory[0xFF06] = 0x66;
    gb.memory[0xFF07] = 0x05;
    gb.memory[0xFF0F] = 0;
    gb_run_cycles(&gb, 16);
    check(gb.memory[0xFF05] == 0x66 && (gb.memory[0xFF0F] & 4),
        "timer overflow reloads TMA and requests interrupt");
    gb_destroy(&gb);
}

static void test_background_render(void)
{
    GameBoy gb;
    if (!make_test_rom(&gb, 2)) {
        check(0, "allocate PPU test ROM");
        return;
    }
    gb.memory[0x8000] = 0xFF;
    gb.memory[0x8001] = 0;
    gb.memory[0xFF47] = 0xE4;
    gb.memory[0xFF40] = 0x91;
    gb_run_cycles(&gb, 252);
    check(gb.framebuffer[0] == 0xFFAAAAAAu,
        "PPU renders tile color into the framebuffer");
    gb_destroy(&gb);
}

int main(void)
{
    test_alu_flags();
    test_mbc1_bank_switch();
    test_timer_overflow();
    test_background_render();

    if (failures) {
        fprintf(stderr, "%u core test(s) failed.\n", failures);
        return 1;
    }
    puts("All core tests passed.");
    return 0;
}
