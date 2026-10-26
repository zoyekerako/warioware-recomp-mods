/* Drives the decompiled game code as far as it currently goes.
 *
 * AgbMain itself is still assembly, so this stands in for it: bring up SDL,
 * initialise the runtime, then call the game's own init path and step the
 * frame loop.  Every function that has not been decompiled yet is a stub that
 * names itself and stops, which makes this a progress meter -- it gets further
 * each time another subsystem lands.
 */
#include "gba_hw.h"

#include "../renderer/gba_ppu.h"

#include <SDL.h>
#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* A fault here almost always means a data stub: the game followed a pointer
   out of a table that has not been converted from the ROM yet, and the zeroed
   stub gave it NULL.  Without this the process just dies with signal 11 and
   says nothing; with it the call path names the scene that wanted the data,
   which is the same service the function stubs provide.

   Only async-signal-safe calls here -- write(2) and backtrace_symbols_fd. */
static void wwpc_hex(unsigned long v)
{
    static const char digits[] = "0123456789ABCDEF";
    char buf[19];
    int i;

    buf[0] = '0';
    buf[1] = 'x';
    for (i = 0; i < 16; i++) {
        buf[17 - i] = digits[(v >> (4 * i)) & 0xF];
    }
    buf[18] = '\n';
    write(2, buf, sizeof buf);
}

static void wwpc_fault(int sig, siginfo_t *info, void *ctx)
{
    static const char msg[] =
        "\n[fault] Speicherzugriffsfehler -- vermutlich ein noch genullter "
        "Datenblock.\nAdresse: ";
    static const char msgpc[] = "Befehlszaehler: ";
    static const char msg2[] = "Aufrufpfad:\n";
    void *frames[24];
    int n;

    write(2, msg, sizeof(msg) - 1);
    wwpc_hex((unsigned long)(info != NULL ? info->si_addr : 0));
#if defined(__APPLE__) && defined(__aarch64__)
    /* backtrace() walks frame pointers and drops leaf frames, so the faulting
       instruction is often missing from the list above.  The saved context has
       it exactly. */
    write(2, msgpc, sizeof(msgpc) - 1);
    wwpc_hex((unsigned long)((ucontext_t *)ctx)->uc_mcontext->__ss.__pc);
#else
    (void)msgpc;
#endif
    write(2, msg2, sizeof(msg2) - 1);
    n = backtrace(frames, 24);
    backtrace_symbols_fd(frames, n, 2);
    _exit(139);
    (void)sig;
    (void)ctx;
}

/* Provided by gba_runtime.c, compiled against the game's own headers. */
void wwpc_runtime_init(void);
void update_key_buffers(void);
void func_08000F28(void);
const GbaVideoState *wwpc_video_state(void);

/* The game's init path (src/main.c). */
void func_080001D4(void);

/* AgbMain's loop body, reproduced here because AgbMain itself is still
   assembly -- its preamble is a hardware reset a PC build has no use for, but
   the loop is what actually drives the game. */
void vblank_wait(void);
unsigned short get_random_u16(void);
void update_key_buffers(void);
void func_080F3C78(void);   /* sound driver tick */
void func_080F3A90(void);   /* sound driver frame end */
void func_080F4090(void);   /* sound driver init */
void func_080F3C60(int, int, int, int);

void func_0800DB04(void);
void title_scene_run(void);
void gameplay_scene_run(void);
void func_0800DE24(void);
void main_menu_scene_run(void);
void func_080166AC(void);
void soft_reset_scene_run(void);
void func_08016E6C(void);

extern short gCurrentScene;
extern short D_030035E0;
extern unsigned short gCurrentKeys;
extern unsigned char D_03003638;
extern unsigned short D_03003840;

static void run_current_scene(void)
{
    switch (gCurrentScene) {
    case 0:    func_0800DB04();        break;
    case 1:    title_scene_run();      break;
    case 2:    gameplay_scene_run();   break;
    case 3:    func_0800DE24();        break;
    case 4:    main_menu_scene_run();  break;
    case 5:    func_080166AC();        break;
    case 6:    soft_reset_scene_run(); break;
    case 7:    func_08016E6C();        break;
    case 0x63: gCurrentScene = 1;      break;
    default:                           break;
    }
}

static const struct { SDL_Keycode key; int button; } key_map[] = {
    { SDLK_x,      GBA_KEY_A },      { SDLK_z,      GBA_KEY_B },
    { SDLK_RETURN, GBA_KEY_START },  { SDLK_RSHIFT, GBA_KEY_SELECT },
    { SDLK_a,      GBA_KEY_L },      { SDLK_s,      GBA_KEY_R },
    { SDLK_UP,     GBA_KEY_UP },     { SDLK_DOWN,   GBA_KEY_DOWN },
    { SDLK_LEFT,   GBA_KEY_LEFT },   { SDLK_RIGHT,  GBA_KEY_RIGHT },
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

int main(int argc, char **argv)
{
    int scale = 3;
    long frameLimit = 0;
    /* The init scene always hands over to the intro, whose data still lives in
       an unconverted ROM block.  --scene starts somewhere else instead; scene 1
       is the title, the one scene whose data is fully in C today. */
    int startScene = -1;
    long framesRun = 0;
    int argi;

    for (argi = 1; argi < argc; argi++) {
        if (strcmp(argv[argi], "--frames") == 0 && argi + 1 < argc) {
            frameLimit = strtol(argv[++argi], NULL, 10);
        } else if (strcmp(argv[argi], "--scene") == 0 && argi + 1 < argc) {
            startScene = (int)strtol(argv[++argi], NULL, 0);
        } else {
            scale = atoi(argv[argi]);
        }
    }
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    uint32_t *frame;
    int running = 1;

    {
        struct sigaction sa;

        memset(&sa, 0, sizeof sa);
        sa.sa_sigaction = wwpc_fault;
        sa.sa_flags = SA_SIGINFO;
        sigaction(SIGSEGV, &sa, NULL);
        sigaction(SIGBUS, &sa, NULL);
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    window = SDL_CreateWindow("WarioWare Inc. -- Spielcode",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              GBA_SCREEN_W * scale, GBA_SCREEN_H * scale,
                              SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    SDL_RenderSetLogicalSize(renderer, GBA_SCREEN_W, GBA_SCREEN_H);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                GBA_SCREEN_W, GBA_SCREEN_H);
    frame = malloc(sizeof *frame * GBA_SCREEN_W * GBA_SCREEN_H);

    wwpc_runtime_init();

    printf("Rufe func_080001D4 (Spiel-Initialisierung) auf ...\n");
    fflush(stdout);
    func_080001D4();
    printf("Initialisierung durchgelaufen.\n");
    fflush(stdout);

    func_080F4090();
    func_080F3C60(0x23, 2, 2, 4);

    gCurrentScene = 0;
    if (startScene >= 0) {
        gCurrentScene = (short)startScene;
        printf("Starte bei Szene %d.\n", startScene);
        fflush(stdout);
    }
    D_030035E0 = 1;   /* first frame of the scene, so it starts itself up */

    while (running) {
        SDL_Event ev;

        gba_hw_frame_begin();
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT ||
                (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE)) {
                running = 0;
            }
            if (ev.type == SDL_KEYDOWN) {
                apply_key(ev.key.keysym.sym, 1);
            }
            if (ev.type == SDL_KEYUP) {
                apply_key(ev.key.keysym.sym, 0);
            }
        }

        func_08000F28();   /* stands in for the VBlank interrupt */

        vblank_wait();
        get_random_u16();
        func_080F3C78();
        update_key_buffers();
        D_03003840++;

        {
            short previous = gCurrentScene;

            run_current_scene();

            if (D_03003638 != 0 && (gCurrentKeys & 0xF) == 0xF) {
                gCurrentScene = 6;   /* A+B+Select+Start soft-resets */
            }
            D_030035E0 = (gCurrentScene == previous) ? 0 : 1;
        }

        func_080F3A90();

        gba_ppu_render(wwpc_video_state(), gba_vram, gba_oam, frame);

        SDL_UpdateTexture(texture, NULL, frame,
                          GBA_SCREEN_W * (int)sizeof *frame);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
        gba_hw_frame_wait();

        if (frameLimit && ++framesRun >= frameLimit) {
            running = 0;
        }
    }

    printf("%ld Bilder gelaufen.\n", framesRun);

    SDL_Quit();
    return 0;
}
