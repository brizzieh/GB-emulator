#include "gb_internal.h"

#define INTERRUPT_STAT 0x02
#define INTERRUPT_VBLANK 0x01

void gb_ppu_update_stat(GameBoy *gb)
{
    uint8_t stat = gb->memory[0xFF41];
    uint8_t ly = gb->memory[0xFF44];
    uint8_t mode = stat & 3;
    uint8_t coincidence = ly == gb->memory[0xFF45];
    uint8_t signal = 0;

    stat = (uint8_t)((stat & 0xFC) | mode | (coincidence ? 4 : 0));
    gb->memory[0xFF41] = stat;
    if ((stat & 0x40) && coincidence) signal = 1;
    if ((stat & 0x20) && mode == 2) signal = 1;
    if ((stat & 0x10) && mode == 1) signal = 1;
    if ((stat & 0x08) && mode == 0) signal = 1;
    if (signal && !gb->stat_irq_line) gb->memory[0xFF0F] |= INTERRUPT_STAT;
    gb->stat_irq_line = signal;
}

static uint32_t shade_color(uint8_t palette, uint8_t color)
{
    static const uint32_t shades[4] = {
        0xFFFFFFFFu, 0xFFAAAAAAu, 0xFF555555u, 0xFF000000u
    };
    return shades[(palette >> (color * 2)) & 3];
}

static uint8_t tile_color(GameBoy *gb, uint8_t tile, unsigned pixel_x, unsigned pixel_y)
{
    int address;
    if (gb->memory[0xFF40] & 0x10) {
        address = 0x8000 + (int)tile * 16;
    } else {
        address = 0x9000 + (int)(int8_t)tile * 16;
    }
    uint8_t low = gb->memory[address + (int)pixel_y * 2];
    uint8_t high = gb->memory[address + (int)pixel_y * 2 + 1];
    unsigned bit = 7 - pixel_x;
    return (uint8_t)((((high >> bit) & 1) << 1) | ((low >> bit) & 1));
}

static void render_line(GameBoy *gb, unsigned line)
{
    uint8_t lcdc = gb->memory[0xFF40];
    uint8_t background[GB_SCREEN_WIDTH];
    uint32_t *pixels = &gb->framebuffer[line * GB_SCREEN_WIDTH];
    int window_x = (int)gb->memory[0xFF4B] - 7;
    int window_y = gb->memory[0xFF4A];

    for (unsigned screen_x = 0; screen_x < GB_SCREEN_WIDTH; ++screen_x) {
        uint8_t color = 0;
        if ((lcdc & 1) && ((lcdc & 0x20) == 0 || (int)line < window_y || (int)screen_x < window_x)) {
            unsigned map = (lcdc & 0x08) ? 0x9C00u : 0x9800u;
            unsigned pixel_x = (screen_x + gb->memory[0xFF43]) & 255u;
            unsigned pixel_y = (line + gb->memory[0xFF42]) & 255u;
            uint8_t tile = gb->memory[map + (pixel_y >> 3) * 32 + (pixel_x >> 3)];
            color = tile_color(gb, tile, pixel_x & 7, pixel_y & 7);
        } else if ((lcdc & 1) && (lcdc & 0x20) && (int)line >= window_y && (int)screen_x >= window_x) {
            unsigned map = (lcdc & 0x40) ? 0x9C00u : 0x9800u;
            unsigned pixel_x = (unsigned)((int)screen_x - window_x);
            unsigned pixel_y = line - (unsigned)window_y;
            uint8_t tile = gb->memory[map + (pixel_y >> 3) * 32 + (pixel_x >> 3)];
            color = tile_color(gb, tile, pixel_x & 7, pixel_y & 7);
        }
        background[screen_x] = color;
        pixels[screen_x] = shade_color(gb->memory[0xFF47], color);
    }

    if (!(lcdc & 2)) return;

    unsigned object_height = (lcdc & 4) ? 16u : 8u;
    unsigned selected[10];
    unsigned selected_count = 0;
    for (unsigned sprite_index = 0; sprite_index < 40 && selected_count < 10; ++sprite_index) {
        int top = (int)gb->memory[0xFE00 + sprite_index * 4] - 16;
        if ((int)line >= top && (int)line < top + (int)object_height) {
            selected[selected_count++] = sprite_index;
        }
    }

    uint8_t object_color[GB_SCREEN_WIDTH] = {0};
    uint8_t object_flags[GB_SCREEN_WIDTH] = {0};
    uint8_t object_present[GB_SCREEN_WIDTH] = {0};
    for (unsigned selected_index = 0; selected_index < selected_count; ++selected_index) {
        unsigned sprite_index = selected[selected_index];
        int top = (int)gb->memory[0xFE00 + sprite_index * 4] - 16;
        int left = (int)gb->memory[0xFE01 + sprite_index * 4] - 8;
        uint8_t tile = gb->memory[0xFE02 + sprite_index * 4];
        uint8_t flags = gb->memory[0xFE03 + sprite_index * 4];
        unsigned row = (unsigned)((int)line - top);
        if (flags & 0x40) row = object_height - 1 - row;
        if (object_height == 16) tile &= 0xFE;
        tile = (uint8_t)(tile + row / 8);
        row &= 7;
        for (unsigned column = 0; column < 8; ++column) {
            int screen_x = left + (int)column;
            if (screen_x < 0 || screen_x >= GB_SCREEN_WIDTH || object_present[screen_x]) continue;
            unsigned tile_x = (flags & 0x20) ? 7 - column : column;
            uint8_t color = tile_color(gb, tile, tile_x, row);
            if (color == 0) continue;
            object_present[screen_x] = 1;
            object_color[screen_x] = color;
            object_flags[screen_x] = flags;
        }
    }

    for (unsigned screen_x = 0; screen_x < GB_SCREEN_WIDTH; ++screen_x) {
        if (!object_present[screen_x]) continue;
        if ((object_flags[screen_x] & 0x80) && background[screen_x] != 0) continue;
        uint8_t palette = (object_flags[screen_x] & 0x10)
            ? gb->memory[0xFF49] : gb->memory[0xFF48];
        pixels[screen_x] = shade_color(palette, object_color[screen_x]);
    }
}

void gb_ppu_tick(GameBoy *gb)
{
    if (!(gb->memory[0xFF40] & 0x80)) {
        gb->ppu_dot = 0;
        gb->memory[0xFF44] = 0;
        gb->memory[0xFF41] &= 0xFC;
        gb_ppu_update_stat(gb);
        return;
    }

    gb->ppu_dot++;
    uint8_t ly = gb->memory[0xFF44];
    uint8_t mode;
    if (ly >= 144) {
        mode = 1;
    } else if (gb->ppu_dot < 80) {
        mode = 2;
    } else if (gb->ppu_dot < 252) {
        mode = 3;
    } else {
        mode = 0;
        if (gb->ppu_dot == 252) render_line(gb, ly);
    }
    gb->memory[0xFF41] = (uint8_t)((gb->memory[0xFF41] & 0xFC) | mode);

    if (gb->ppu_dot >= 456) {
        gb->ppu_dot = 0;
        ly++;
        if (ly == 144) {
            gb->memory[0xFF0F] |= INTERRUPT_VBLANK;
            gb->frames++;
        } else if (ly > 153) {
            ly = 0;
        }
        gb->memory[0xFF44] = ly;
    }
    gb_ppu_update_stat(gb);
}
