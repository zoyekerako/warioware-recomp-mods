/* The PC shell.
 *
 * This is the frame loop the GBA's hardware used to drive: read the keys,
 * advance the sprite pipeline, composite VRAM and OAM into a framebuffer, put
 * it on screen, and wait out the rest of the 59.7275 Hz frame.  The compositor
 * and the sprite engine are the same code the headless verification tools use,
 * so what appears here is what the ROM's own pipeline would have produced.
 */
#include "assets_io.h"
#include "gba_hw.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SPRITES 200
#define MAX_OBJ 128
#define DEFAULT_SCALE 3

/* Keyboard to GBA buttons.  The D-pad doubles on the arrow keys and WASD. */
static const struct { SDL_Keycode key; int button; } key_map[] = {
    { SDLK_x,      GBA_KEY_A },      { SDLK_z,     GBA_KEY_B },
    { SDLK_RETURN, GBA_KEY_START },  { SDLK_RSHIFT, GBA_KEY_SELECT },
    { SDLK_a,      GBA_KEY_L },      { SDLK_s,     GBA_KEY_R },
    { SDLK_UP,     GBA_KEY_UP },     { SDLK_DOWN,  GBA_KEY_DOWN },
    { SDLK_LEFT,   GBA_KEY_LEFT },   { SDLK_RIGHT, GBA_KEY_RIGHT },
};

/* Where the title screen's animated pieces sit, and how deep. */
static const struct { const char *anim; int x, y; unsigned z; } title_actors[] = {
    { "title_anim_press_start",    120, 120, 0x1000 },
    { "title_anim_trash_can",       40,  60, 0x2000 },
    { "title_anim_hourglass",      200,  60, 0x2000 },
    { "title_anim_treasure_chest",  40, 110, 0x3000 },
    { "title_anim_purple_gem",     200, 110, 0x3000 },
};

static void apply_key(SDL_Keycode key, int down)
{
    size_t i;

    for (i = 0; i < sizeof key_map / sizeof key_map[0]; i++) {
        if (key_map[i].key == key) {
            gba_hw_set_key(key_map[i].button, down);
            return;
        }
    }
}

static void usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s [scene.bin] [anim.bin] [scale] [--frames N]\n"
            "  --frames N   run N frames and exit; with SDL_VIDEODRIVER=dummy\n"
            "               this is a non-interactive smoke test\n", argv0);
}

int main(int argc, char **argv)
{
    const char *scenePath = "assets/title_scene.bin";
    const char *animPath  = "assets/title_anim.bin";
    int scale = DEFAULT_SCALE;
    long frameLimit = 0;
    int positional = 0;
    long framesRun = 0;
    int argi;

    for (argi = 1; argi < argc; argi++) {
        if (strcmp(argv[argi], "--frames") == 0 && argi + 1 < argc) {
            frameLimit = strtol(argv[++argi], NULL, 10);
        } else if (strcmp(argv[argi], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else if (argv[argi][0] == '-') {
            usage(argv[0]);
            return 2;
        } else {
            switch (positional++) {
            case 0: scenePath = argv[argi]; break;
            case 1: animPath = argv[argi]; break;
            default: scale = atoi(argv[argi]); break;
            }
        }
    }

    SceneBlob scene;
    AnimBank bank;
    SpriteHandler handler;
    Sprite *sprites;
    uint32_t *oam;
    uint32_t *frame;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    int running = 1, animate = 1;
    Uint64 startedAt, frameWorkStart, workTicks = 0;
    size_t i;

    if (scene_blob_load(&scene, scenePath) != 0) {
        return 1;
    }
    if (anim_bank_load(&bank, animPath) != 0) {
        scene_blob_free(&scene);
        return 1;
    }

    sprites = calloc(MAX_SPRITES, sizeof *sprites);
    oam = calloc(MAX_OBJ * 2, sizeof *oam);
    frame = malloc(sizeof *frame * GBA_SCREEN_W * GBA_SCREEN_H);
    if (!sprites || !oam || !frame) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    sprite_handler_create(&handler, MAX_OBJ, oam, MAX_SPRITES, sprites);

    for (i = 0; i < sizeof title_actors / sizeof title_actors[0]; i++) {
        int a = anim_bank_find(&bank, title_actors[i].anim);

        if (a >= 0) {
            sprite_create(&handler, bank.anims[a],
                          (int16_t)title_actors[i].x,
                          (int16_t)title_actors[i].y,
                          (uint16_t)title_actors[i].z);
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    window = SDL_CreateWindow("WarioWare Inc. -- PC-Port",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              GBA_SCREEN_W * scale, GBA_SCREEN_H * scale,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return 1;
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        /* Headless drivers have no accelerated backend; software still gives a
           correct frame, which is all the smoke test needs. */
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        return 1;
    }
    /* Nearest-neighbour: the source is a 240x160 tile grid, and any filtering
       turns the one-pixel outlines the artwork relies on into mush. */
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_RenderSetLogicalSize(renderer, GBA_SCREEN_W, GBA_SCREEN_H);

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                GBA_SCREEN_W, GBA_SCREEN_H);
    if (!texture) {
        fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
        return 1;
    }

    startedAt = SDL_GetPerformanceCounter();

    while (running) {
        SDL_Event ev;

        gba_hw_frame_begin();
        frameWorkStart = SDL_GetPerformanceCounter();

        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_QUIT:
                running = 0;
                break;
            case SDL_KEYDOWN:
                if (ev.key.keysym.sym == SDLK_ESCAPE) {
                    running = 0;
                } else if (ev.key.keysym.sym == SDLK_SPACE && !ev.key.repeat) {
                    animate = !animate;   /* freeze a frame to inspect it */
                }
                apply_key(ev.key.keysym.sym, 1);
                break;
            case SDL_KEYUP:
                apply_key(ev.key.keysym.sym, 0);
                break;
            default:
                break;
            }
        }

        if (animate) {
            memset(oam, 0, MAX_OBJ * 2 * sizeof *oam);
            sprite_engine_emit(&handler);
        }

        gba_ppu_render(&scene.video, scene.vram, (const uint8_t *)oam, frame);

        SDL_UpdateTexture(texture, NULL, frame,
                          GBA_SCREEN_W * (int)sizeof *frame);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        workTicks += SDL_GetPerformanceCounter() - frameWorkStart;
        gba_hw_frame_wait();

        if (frameLimit && ++framesRun >= frameLimit) {
            running = 0;
        }
    }

    if (frameLimit) {
        double seconds = (double)(SDL_GetPerformanceCounter() - startedAt) /
                         (double)SDL_GetPerformanceFrequency();

        printf("%ld frames in %.3f s (%.2f Hz, target %.4f Hz)\n",
               framesRun, seconds, framesRun / seconds, GBA_FRAME_HZ);
        printf("  work %.3f ms/frame, budget %.3f ms\n",
               (double)workTicks / (double)SDL_GetPerformanceFrequency()
                   * 1000.0 / (double)framesRun,
               1000.0 / GBA_FRAME_HZ);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    free(frame);
    free(oam);
    free(sprites);
    anim_bank_free(&bank);
    scene_blob_free(&scene);
    return 0;
}
