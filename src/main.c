#include "gb.h"

#include <SDL.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYCLES_PER_FRAME 70224u

static void print_usage(const char *program)
{
    fprintf(stderr,
        "Usage: %s <rom.gb> [--headless] [--frames count]\n"
        "Controls: arrows = D-pad, Z = A, X = B, Backspace = Select, Enter = Start\n",
        program);
}

static int parse_frames(const char *text, uint64_t *frames)
{
    char *end = NULL;
    errno = 0;
    unsigned long long parsed = strtoull(text, &end, 10);
    if (errno || !end || *end || parsed == 0 || parsed > 100000) return 0;
    *frames = (uint64_t)parsed;
    return 1;
}

static size_t count_nonwhite_pixels(const GameBoy *gb)
{
    size_t count = 0;
    for (size_t pixel = 0; pixel < GB_SCREEN_PIXELS; ++pixel) {
        if (gb->framebuffer[pixel] != 0xFFFFFFFFu) count++;
    }
    return count;
}

static void handle_key(GameBoy *gb, SDL_Keycode key, int pressed)
{
    unsigned button;
    switch (key) {
        case SDLK_RIGHT: button = 0; break;
        case SDLK_LEFT: button = 1; break;
        case SDLK_UP: button = 2; break;
        case SDLK_DOWN: button = 3; break;
        case SDLK_z: button = 4; break;
        case SDLK_x: button = 5; break;
        case SDLK_BACKSPACE: button = 6; break;
        case SDLK_RETURN: button = 7; break;
        default: return;
    }
    gb_set_key(gb, button, pressed);
}

int main(int argc, char **argv)
{
    const char *rom_path = NULL;
    uint64_t frames = 120;
    int headless = 0;

    for (int argument = 1; argument < argc; ++argument) {
        if (strcmp(argv[argument], "--headless") == 0) {
            headless = 1;
        } else if (strcmp(argv[argument], "--frames") == 0) {
            if (argument + 1 >= argc || !parse_frames(argv[++argument], &frames)) {
                print_usage(argv[0]);
                return 2;
            }
        } else if (argv[argument][0] == '-') {
            print_usage(argv[0]);
            return 2;
        } else if (!rom_path) {
            rom_path = argv[argument];
        } else {
            print_usage(argv[0]);
            return 2;
        }
    }

    if (!rom_path) {
        print_usage(argv[0]);
        return 2;
    }

    GameBoy gb;
    gb_init(&gb);
    if (!gb_load_rom(&gb, rom_path)) {
        gb_destroy(&gb);
        return 1;
    }

    if (headless) {
        gb_run_cycles(&gb, frames * CYCLES_PER_FRAME);
        printf("Headless run: requested_frames=%" PRIu64
               " actual_vblanks=%" PRIu64
               " cycles=%" PRIu64
               " instructions=%" PRIu64
               " pc=%04X ly=%u lcdc=%02X hash=%08" PRIX32
               " nonwhite_pixels=%zu illegal_opcode=%02X\n",
               frames, gb.frames, gb.cycles, gb.instructions,
               gb.pc, gb.memory[0xFF44], gb.memory[0xFF40],
               gb_frame_hash(&gb), count_nonwhite_pixels(&gb), gb.illegal_opcode);
        int result = gb.illegal_opcode ? 1 : 0;
        gb_destroy(&gb);
        return result;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        gb_destroy(&gb);
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("Game Boy DMG",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        GB_SCREEN_WIDTH * 3, GB_SCREEN_HEIGHT * 3, SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = window
        ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED)
        : NULL;
    if (window && !renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    SDL_Texture *texture = renderer
        ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT)
        : NULL;

    if (!window || !renderer || !texture) {
        fprintf(stderr, "SDL setup failed: %s\n", SDL_GetError());
        if (texture) SDL_DestroyTexture(texture);
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
        gb_destroy(&gb);
        return 1;
    }

    int running = 1;
    while (running && !gb.illegal_opcode) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                    running = 0;
                }
                handle_key(&gb, event.key.keysym.sym, event.type == SDL_KEYDOWN);
            }
        }

        gb_run_cycles(&gb, CYCLES_PER_FRAME);
        SDL_UpdateTexture(texture, NULL, gb.framebuffer,
            GB_SCREEN_WIDTH * (int)sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    if (gb.illegal_opcode) {
        fprintf(stderr, "Emulation stopped on illegal opcode 0x%02X.\n", gb.illegal_opcode);
    }
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    gb_destroy(&gb);
    return gb.illegal_opcode ? 1 : 0;
}
