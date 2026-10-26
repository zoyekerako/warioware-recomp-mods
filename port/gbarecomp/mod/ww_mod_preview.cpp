// ww_mod_preview.cpp -- rendert das Mod-Overlay ohne laufendes Spiel.
//
// Menue und Hintergrund erscheinen sonst nur in einem SDL-Fenster; damit
// ihr Aussehen trotzdem pruefbar ist (Schrift, Ausrichtung, Massstab,
// Drehung), bindet dieses Werkzeug ww_mod.cpp direkt ein, stellt einen
// Attrappen-Bus daneben und schreibt das Ergebnis als PPM.
//
//   c++ -std=c++17 -I <gbarecomp>/src/armv4t -o preview ww_mod_preview.cpp
//   ./preview aus.ppm <breite> <hoehe> [spielbild.ppm] [drehung] [cursor]
//
// Ohne -DGBARECOMP_HAVE_SDL2 gebaut: f10_edge() ist dann wirkungslos, was
// fuer eine reine Zeichenprobe genau richtig ist.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

// ── Attrappen fuer die Laufzeit ───────────────────────────────────────
#include "runtime_arm_types.h"
extern "C" ArmCpuState g_cpu;
ArmCpuState g_cpu;
extern "C" void (*g_runtime_fn_entry_hook)(uint32_t);
void (*g_runtime_fn_entry_hook)(uint32_t) = nullptr;
extern "C" unsigned long long g_runtime_vblank_starts;
unsigned long long g_runtime_vblank_starts = 0;

// Ein winziger Gastspeicher: nur die Felder, die das Overlay liest. Dazu die
// echte ROM, sobald ein Pfad genannt ist -- die Menuetabellen des Spiels
// stehen dort, und der Selbsttest soll gegen sie rechnen, nicht gegen
// nachgebaute Zahlen.
static uint8_t iwram[0x8000];
static std::vector<uint8_t> g_rom;
extern "C" {
uint8_t  bus_read_u8 (uint32_t a) {
    if (a >= 0x03000000u && a < 0x03008000u) return iwram[a - 0x03000000u];
    if (a >= 0x08000000u && a - 0x08000000u < g_rom.size())
        return g_rom[a - 0x08000000u];
    return 0;
}
uint16_t bus_read_u16(uint32_t a) {
    return static_cast<uint16_t>(bus_read_u8(a) | (bus_read_u8(a + 1) << 8));
}
uint32_t bus_read_u32(uint32_t a) {
    return static_cast<uint32_t>(bus_read_u16(a)) |
           (static_cast<uint32_t>(bus_read_u16(a + 2)) << 16);
}
void bus_write_u8 (uint32_t a, uint8_t v) {
    if (a >= 0x03000000u && a < 0x03008000u) iwram[a - 0x03000000u] = v;
}
void bus_write_u16(uint32_t a, uint16_t v) { bus_write_u8(a, uint8_t(v)); bus_write_u8(a + 1, uint8_t(v >> 8)); }
void bus_write_u32(uint32_t a, uint32_t v) { bus_write_u16(a, uint16_t(v)); bus_write_u16(a + 2, uint16_t(v >> 16)); }
void runtime_dispatch(uint32_t) {}
void runtime_call_push_return(uint32_t) {}
void runtime_call_cancel_return(uint32_t) {}
}

static bool load_rom(const char* path) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n <= 0) { std::fclose(f); return false; }
    g_rom.assign(static_cast<size_t>(n), 0);
    const size_t got = std::fread(g_rom.data(), 1, g_rom.size(), f);
    std::fclose(f);
    if (got != g_rom.size()) { g_rom.clear(); return false; }
    return true;
}

#include "ww_mod.cpp"

static bool read_ppm(const char* path, std::vector<uint8_t>* px, int* w, int* h) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    char magic[3] = {};
    int maxv = 0;
    if (std::fscanf(f, "%2s %d %d %d", magic, w, h, &maxv) != 4 ||
        std::strcmp(magic, "P6") != 0) { std::fclose(f); return false; }
    std::fgetc(f);
    px->assign(static_cast<size_t>(*w) * *h * 3, 0);
    const size_t got = std::fread(px->data(), 1, px->size(), f);
    std::fclose(f);
    return got == px->size();
}

// ── Selbsttest der Zeigerrechnung ─────────────────────────────────────
//
// Zwei Dinge werden geprueft, beide ohne Hand am Zeiger:
//
//  1. Hin und zurueck. Ein Spielpunkt wird vorwaerts in die Zeichenflaeche
//     und ins Fenster gerechnet, dann mit window_to_game zurueck. Faellt er
//     auf sich selbst, stimmen Drehung, Massstab, Zentrierung und die
//     Fensterabbildung zusammen.
//  2. Die Zustandsmaschine. Aufsetzen, Ziehen, Loslassen -- und welche
//     Tasten dabei herauskommen.
static int selftest() {
    const int gw = 240, gh = 160;
    std::vector<uint8_t> game(static_cast<size_t>(gw) * gh * 3, 0x40);
    int bad = 0, checked = 0;

    const int wins[][2] = {{1600, 900}, {1024, 768}, {720, 1280}, {480, 320}};
    const char* modes[] = {"Sharp", "Fit", "Stretch"};

    for (int rot = 0; rot < 4; ++rot) {
        for (int mode = 0; mode < 3; ++mode) {
            for (const auto& wv : wins) {
                g.rotation = rot;
                g.screen = mode;
                g.wide = kWideOff;
                int sw = 0, sh = 0;
                if (!ww_mod_surface_size(wv[0], wv[1], &sw, &sh)) { ++bad; continue; }
                std::vector<uint8_t> out(static_cast<size_t>(sw) * sh * 3, 0);
                g.open = false;
                ww_mod_compose(game.data(), gw, gh, out.data(), sw, sh);

                const gbarecomp::PresentationLayout lay =
                    gbarecomp::compute_presentation_layout(wv[0], wv[1], sw, sh);
                int worst = 0;
                for (int gy = 4; gy < gh; gy += 23) {
                    for (int gx = 4; gx < gw; gx += 29) {
                        // vorwaerts: Spiel -> gedreht -> Flaeche -> Fenster
                        const bool swap = (rot & 1) != 0;
                        const int rw = swap ? gh : gw, rh = swap ? gw : gh;
                        int rx = 0, ry = 0;
                        rotate_fwd(rot, gx, gy, gw, gh, &rx, &ry);
                        const int sx = g.geo_dx + rx * g.geo_dw / rw;
                        const int sy = g.geo_dy + ry * g.geo_dh / rh;
                        const int wx = lay.x + sx * lay.width / sw;
                        const int wy = lay.y + sy * lay.height / sh;
                        // zurueck
                        int bx = 0, by = 0;
                        ++checked;
                        if (!window_to_game(wx, wy, &bx, &by)) { ++bad; continue; }
                        const int e = std::max(std::abs(bx - gx), std::abs(by - gy));
                        if (e > worst) worst = e;
                        if (e > 1) ++bad;
                    }
                }
                std::printf("  Drehung %3d  %-7s  Fenster %4dx%-4d  Flaeche %4dx%-4d"
                            "  groesster Fehler %d\n",
                            rot * 90, modes[mode], wv[0], wv[1], sw, sh, worst);
            }
        }
    }
    std::printf("\n  %d Punkte geprueft, %d daneben\n\n", checked, bad);

    // ── Zustandsmaschine ──────────────────────────────────────────────
    g.rotation = 0; g.screen = kScreenSharp; g.wide = kWideOff;
    int sw = 0, sh = 0;
    ww_mod_surface_size(960, 640, &sw, &sh);
    std::vector<uint8_t> out(static_cast<size_t>(sw) * sh * 3, 0);
    ww_mod_compose(game.data(), gw, gh, out.data(), sw, sh);

    auto name = [](uint16_t b) {
        static char s[48]; s[0] = 0;
        if (b & kKeyA) std::strcat(s, "A ");
        if (b & kKeyUp) std::strcat(s, "Up ");
        if (b & kKeyDown) std::strcat(s, "Down ");
        if (b & kKeyLeft) std::strcat(s, "Left ");
        if (b & kKeyRight) std::strcat(s, "Right ");
        if (!s[0]) std::strcpy(s, "-");
        return s;
    };
    // Mitte des Fensters, dann nach rechts ziehen.
    const int cx = 480, cy = 320, px = 4;   // 960/240 = 4 Fensterpunkte je Spielpunkt
    struct Step { int dx; bool down; const char* what; };
    const Step drag[] = {
        {0, false, "losgelassen"}, {0, true, "aufgesetzt"},
        {2 * px, true, "2 Punkte"}, {6 * px, true, "6 Punkte"},
        {20 * px, true, "20 Punkte"}, {20 * px, true, "gehalten"},
        {20 * px, false, "losgelassen"},
    };
    for (int scheme : {kTouchBoth, kTouchFlick, kTouchHold}) {
        std::printf("  Schema %s:\n",
                    scheme == kTouchBoth  ? "both (tippen+ziehen)" :
                    scheme == kTouchFlick ? "flick (wischen)" : "hold (halten)");
        g.pointer_was = false; g.touch_press_left = 0; g.touch_dir_left = 0;
        g.flick_fired = false;
        for (const auto& st : drag) {
            const uint16_t b = pointer_keys_from(cx + st.dx, cy, st.down, scheme);
            std::printf("    %-12s -> %s\n", st.what, name(b));
        }
    }
    // ── Menue per Zeiger ──────────────────────────────────────────────
    // ui_step ist rein; die Trefferflaechen kommen aus einem echten
    // ww_mod_compose, also genau aus der Rechnung, mit der gezeichnet wird.
    std::printf("\n  Menue per Zeiger:\n");
    int mbad = 0;
    auto check = [&](bool ok, const char* what) {
        std::printf("    %-52s %s\n", what, ok ? "ok" : "FALSCH");
        if (!ok) ++mbad;
    };
    g.rotation = 0; g.screen = kScreenSharp; g.wide = kWideOff;
    ww_mod_surface_size(960, 640, &sw, &sh);
    out.assign(static_cast<size_t>(sw) * sh * 3, 0);
    auto frame = [&] { ww_mod_compose(game.data(), gw, gh, out.data(), sw, sh); };
    auto tap = [&](int x, int y) {
        ui_step(x, y, true, true, false);
        ui_step(x, y, true, false, false);
    };
    auto row_mid = [&](int item) { return g.ml.row_top[item] + g.ml.row_h / 2; };
    g.inject_left = 0;

    // Oeffnen ueber den Menueknopf
    g.open = false;
    frame();
    check(!g.mbtn.visible, "Knopf ist ohne Zeigerbewegung verborgen");
    ui_step(100, 100, true, false, true);
    frame();
    check(g.mbtn.visible, "Knopf erscheint, sobald sich der Zeiger regt");
    const int bx = (g.mbtn.x0 + g.mbtn.x1) / 2, by = (g.mbtn.y0 + g.mbtn.y1) / 2;
    ui_step(bx, by, true, true, false);
    check(!g.open, "Aufsetzen auf dem Knopf oeffnet noch nicht");
    ui_step(bx, by, true, false, false);
    check(g.open, "Loslassen auf dem Knopf oeffnet das Menue");
    frame();
    const int lab_x = g.ml.px + 30 * g.ml.sc;

    // Zeile waehlen
    tap(lab_x, row_mid(kItemLives));
    check(g.cursor == kItemLives, "Tippen auf eine Zeile waehlt sie");

    // Pfeile und Wert
    const bool lives0 = g.inf_lives;
    frame();
    tap((g.ml.val_x1[kItemLives] + g.ml.px + g.ml.pw) / 2, row_mid(kItemLives));
    check(g.inf_lives != lives0, "Pfeil rechts schaltet um");

    g.seed = 50;
    frame();
    tap((g.ml.arl_x0[kItemSeed] + g.ml.val_x0[kItemSeed]) / 2, row_mid(kItemSeed));
    check(g.seed == 49, "Pfeil links zaehlt herunter");

    g.random_mode = kRandomOff;
    frame();
    tap((g.ml.val_x0[kItemRandom] + g.ml.val_x1[kItemRandom]) / 2, row_mid(kItemRandom));
    check(g.random_mode == kRandomGames, "Wert antippen schaltet weiter");

    // Halten wiederholt, Wegziehen haelt die Wiederholung an
    g.seed = 50;
    frame();
    const int rx = (g.ml.val_x1[kItemSeed] + g.ml.px + g.ml.pw) / 2;
    const int ry = row_mid(kItemSeed);
    for (int i = 0; i < 61; ++i) ui_step(rx, ry, true, true, false);
    ui_step(rx, ry, true, false, false);
    char buf[80];
    std::snprintf(buf, sizeof(buf), "Eine Sekunde halten wiederholt (50 -> %d)", g.seed);
    check(g.seed > 60, buf);

    g.seed = 50;
    ui_step(rx, ry, true, true, false);
    for (int i = 0; i < 60; ++i) ui_step(lab_x, ry, true, true, false);
    ui_step(lab_x, ry, true, false, false);
    check(g.seed == 51, "Weggezogen: keine Wiederholung");

    // Schliessen -- mit Abbruch durch Wegziehen
    frame();
    const int close_x = (g.ml.close_x0 + g.ml.close_x1) / 2;
    const int close_y = (g.ml.close_y0 + g.ml.close_y1) / 2;
    ui_step(close_x, close_y, true, true, false);
    ui_step(lab_x, close_y, true, true, false);
    ui_step(lab_x, close_y, true, false, false);
    check(g.open, "Vom Schliessen-Knopf weggezogen: bleibt offen");
    frame();
    tap(close_x, close_y);
    check(!g.open, "Schliessen-Knopf schliesst");

    g.open = true;
    frame();
    tap(5, 5);
    check(!g.open, "Tippen ausserhalb des Menues schliesst");

    g.open = true;
    frame();
    tap(g.ml.px + g.ml.pw / 2, g.ml.py + 2 * g.ml.sc);
    check(g.open, "Tippen auf den Rand im Menue schliesst nicht");

    // Maus darueber
    g.cursor = 0;
    frame();
    ui_step(lab_x, row_mid(kItemSpeed), true, false, true);
    check(g.cursor == kItemSpeed, "Maus darueber hebt die Zeile hervor");
    ui_step(lab_x, row_mid(kItemSpeed), true, false, false);
    g.cursor = kItemSeed;
    ui_step(lab_x, row_mid(kItemSpeed), true, false, false);
    check(g.cursor == kItemSeed, "Ruhende Maus nimmt dem Steuerkreuz nichts weg");

    // Ein Druck aufs Spiel gehoert dem Spiel
    g.open = false;
    ui_step(100, 100, true, false, true);
    frame();
    ui_step(sw / 2, sh / 2, true, true, false);
    check(g.ui.press_target == kUiGame, "Druck aufs Spielbild gehoert dem Spiel");
    ui_step(sw / 2, sh / 2, true, false, false);
    check(!g.open, "und oeffnet kein Menue");

    std::printf("\n  Menue: %d Pruefungen daneben\n", mbad);

    // ── Die Menues des Spiels ─────────────────────────────────────────
    // Gerechnet wird gegen die echten Tabellen der ROM: welcher Punkt
    // welchen Ordner trifft, und welche Taste der Plan daraus macht. Der
    // Gastspeicher daneben ist nur so weit gefuellt, wie die Pruefung liest.
    std::printf("\n  Menues des Spiels:\n");
    int gbad = 0;
    auto gcheck = [&](bool ok, const char* what) {
        std::printf("    %-52s %s\n", what, ok ? "ok" : "FALSCH");
        if (!ok) ++gbad;
    };
    if (g_rom.empty()) {
        std::printf("    (ohne ROM uebersprungen -- Pfad als zweites Argument)\n");
    } else {
        // Spielstand: Stages 0..10 freigeschaltet, der Rest zu.
        constexpr uint32_t kFakeSave = 0x03005800u;
        bus_write_u32(kSavePtr, kFakeSave);
        for (int i = 0; i < 32; ++i)
            bus_write_u8(kFakeSave + 0x20u + i * 8u, i <= 10 ? 0x02 : 0x00);

        auto set_menu = [&](int state, int tab, int entry, int cx, int cy) {
            bus_write_u16(kCurrentScene, 4);          // GLOBAL_SCENE_MAIN_MENU
            bus_write_u8(kMenuVars + 0, uint8_t(entry));
            bus_write_u8(kMenuVars + 1, uint8_t(state));
            bus_write_u8(kMenuVars + 2, uint8_t(tab));
            bus_write_u8(kMenuVars + 3, uint8_t(cx));
            bus_write_u8(kMenuVars + 4, uint8_t(cy));
            bus_write_u8(gp(kOffMenuOn), 1);
            bus_write_u8(gp(kOffBusy), 0);
            bus_write_u8(gp(kOffBusy + 1), 0);
            bus_write_u8(gp(kOffBusy + 2), 0);
            bus_write_u16(kGfxFlags, 0);
            bus_write_u8(gp(kOffGridCols), 4);        // vier Spalten,
            bus_write_u8(gp(kOffGridMax), 13);        // dreizehn Felder
            bus_write_u16(kBgOfs2X, uint16_t(-88));   // Spalte 0 steht links
        };
        auto hit = [&](int x, int y) { return game_menu_hit(x, y); };
        auto plan_key_for = [&](const GameHit& w, bool open) {
            g.plan.active = true; g.plan.want = w; g.plan.open = open;
            g.plan.budget = 900;
            return plan_step();
        };

        // Untere Leiste
        set_menu(0, 0, 0, 0, 0);
        gcheck(hit(53, 152).kind == kHitTab && hit(53, 152).a == 0, "Leiste: links liegt \"Games\"");
        gcheck(hit(126, 152).a == 1, "Leiste: in der Mitte \"Grid\"");
        gcheck(hit(198, 152).a == 2, "Leiste: rechts \"Options\"");
        gcheck(hit(232, 152).kind == kHitTab, "Leiste: der rechte Rand gehoert noch zu \"Options\"");
        set_menu(1, 0, 0, 0, 0);
        gcheck(hit(232, 152).kind == kHitBack, "im Ordner: rechts unten der Zurueck-Knopf");
        gcheck(hit(126, 152).kind == kHitNone, "im Ordner: wo keine Reiter sind, ist nichts");

        // Ordner
        gcheck(hit(42, 92).kind == kHitFolder && hit(42, 92).a == 5 && hit(42, 92).b == 0,
               "Games: der Punkt (42,92) trifft Ordner 5");
        gcheck(hit(42, 132).a == 0, "Games: darunter liegt Ordner 0");
        gcheck(hit(42, 12).kind == kHitFolder && hit(42, 12).a == 8, "Games: oben Ordner 8");
        gcheck(hit(150, 60).kind == kHitNone, "Games: zwischen den Ordnern ist nichts");
        gcheck(hit(98, 12).kind == kHitNone, "Games: ein gesperrter Platz bleibt leer");
        set_menu(2, 1, 0, 0, 0);
        gcheck(hit(42, 102).kind == kHitFolder && hit(42, 102).a == 1 && hit(42, 102).b == 1,
               "Grid: derselbe Punkt trifft die andere Anordnung");

        // Gitter
        set_menu(3, 1, 1, 0, 0);
        gcheck(hit(110, 24).kind == kHitCell && hit(110, 24).a == 0 && hit(110, 24).b == 0,
               "Gitter: erste Zelle links oben");
        gcheck(hit(158, 100).a == 1 && hit(158, 100).b == 2, "Gitter: mittlere Spalte, dritte Zeile");
        gcheck(hit(206, 24).a == 2 && hit(206, 24).b == 0, "Gitter: rechte Spalte");
        gcheck(hit(110, 140).kind == kHitNone, "Gitter: unter dem Rahmen ist nichts");

        // Optionen und Abfrage
        set_menu(4, 2, 0, 0, 0);
        gcheck(hit(120, 67).kind == kHitOption && hit(120, 67).a == 0, "Optionen: erste Zeile");
        gcheck(hit(120, 99).a == 1 && hit(120, 131).a == 2, "Optionen: zweite und dritte Zeile");
        set_menu(5, 2, 0, 0, 0);
        gcheck(hit(80, 112).kind == kHitDialog && hit(80, 112).a == 0, "Abfrage: links \"OK\"");
        gcheck(hit(170, 112).a == 1, "Abfrage: rechts \"No way!\"");

        // Plan: der Weg zum Reiter
        set_menu(0, 0, 0, 0, 0);
        GameHit t; t.kind = kHitTab; t.a = 2;
        gcheck(plan_key_for(t, true) == kKeyRight, "Plan: zum rechten Reiter geht es nach rechts");
        set_menu(0, 2, 0, 0, 0);
        gcheck(plan_key_for(t, true) == kKeyA, "Plan: am Reiter angekommen wird geoeffnet");
        t.kind = kHitFolder; t.a = 5; t.b = 0;
        set_menu(0, 2, 0, 0, 0);
        gcheck(plan_key_for(t, true) == kKeyLeft, "Plan: fuer einen Games-Ordner erst der Reiter");

        // Plan: der Weg durch das Ordnernetz (0 -> 1 -> 5, beides nach oben)
        set_menu(1, 0, 0, 0, 0);
        gcheck(plan_key_for(t, true) == kKeyUp, "Plan: Ordner 0 -> 5 beginnt nach oben");
        set_menu(1, 0, 1, 0, 0);
        gcheck(plan_key_for(t, true) == kKeyUp, "Plan: und geht weiter nach oben");
        set_menu(1, 0, 5, 0, 0);
        gcheck(plan_key_for(t, true) == kKeyA, "Plan: am Ordner wird geoeffnet");
        set_menu(1, 0, 5, 0, 0);
        gcheck(plan_key_for(t, false) == 0 && !g.plan.active,
               "Plan: ohne Oeffnen endet er am Ordner");

        // Plan: ein gesperrter Ordner ist nicht erreichbar
        t.a = 20;
        set_menu(1, 0, 0, 0, 0);
        gcheck(plan_key_for(t, true) == 0 && !g.plan.active,
               "Plan: ein gesperrter Ordner endet ohne Tastendruck");

        // Plan: das Gitter
        t.kind = kHitCell; t.a = 2; t.b = 1;
        set_menu(3, 1, 1, 0, 0);
        gcheck(plan_key_for(t, true) == kKeyDown || plan_key_for(t, true) == kKeyRight,
               "Plan: im Gitter faengt der Weg an");
        set_menu(3, 1, 1, 2, 1);
        gcheck(plan_key_for(t, true) == kKeyA, "Plan: an der Zelle wird geoeffnet");
        // Die letzte Spalte hat nur die unterste Zeile. Sie steht erst im
        // Bild, wenn der Hintergrund dorthin gescrollt ist (func_080136F4:
        // -88 + (Spalten-3)*48), und sie fuellt die Hoehe als ein Feld.
        set_menu(3, 1, 1, 0, 0);
        bus_write_u8(gp(kOffGridMax), 16);
        bus_write_u16(kBgOfs2X, uint16_t(-40));
        gcheck(hit(206, 24).kind == kHitCell && hit(206, 24).a == 3 && hit(206, 24).b == 3,
               "Gitter: die letzte Spalte kennt nur die unterste Zeile");
        bus_write_u8(gp(kOffGridMax), 14);
        gcheck(hit(206, 24).kind == kHitNone, "Gitter: ein Feld jenseits des Endes gibt es nicht");
        bus_write_u8(gp(kOffGridMax), 13);
        bus_write_u16(kBgOfs2X, uint16_t(-88));

        // Plan: nicht bereit heisst warten, und der Vorrat laeuft ab
        set_menu(1, 0, 0, 0, 0);
        bus_write_u16(kGfxFlags, 0x0002);
        t.kind = kHitFolder; t.a = 5; t.b = 0;
        gcheck(plan_key_for(t, true) == 0 && g.plan.active,
               "Plan: waehrend das Bild blendet wird gewartet");
        g.plan.budget = 1;
        gcheck(plan_step() == 0 && !g.plan.active, "Plan: der Vorrat begrenzt das Warten");
        bus_write_u16(kGfxFlags, 0);

        // Druck und Loslassen
        set_menu(0, 0, 0, 0, 0);
        g.plan.active = false; g.game_ptr_down = false; g.press_hit = GameHit{};
        game_menu_pointer(42, 92, true, true);
        gcheck(g.plan.active && !g.plan.open && g.plan.want.a == 5,
               "Druck waehlt den Ordner, oeffnet ihn aber nicht");
        game_menu_pointer(150, 60, true, false);
        gcheck(g.plan.active && !g.plan.open, "Weggezogen und losgelassen: nur die Auswahl");
        // Ein liegen gebliebener Finger zaehlt, auch wenn sich darunter etwas
        // verschoben hat -- sonst ginge ein Tipp im scrollenden Gitter verloren.
        g.game_ptr_down = false; g.press_hit = GameHit{}; g.plan.active = false;
        set_menu(3, 1, 1, 0, 0);
        game_menu_pointer(110, 24, true, true);
        bus_write_u16(kBgOfs2X, uint16_t(-40));
        game_menu_pointer(112, 26, true, false);
        gcheck(g.plan.active && g.plan.open && g.plan.want.a == 0 && g.plan.want.b == 0,
               "Gescrollt und losgelassen: der Tipp gilt dem Feld von vorhin");
        bus_write_u16(kBgOfs2X, uint16_t(-88));
        set_menu(0, 0, 0, 0, 0);
        g.game_ptr_down = false; g.press_hit = GameHit{}; g.plan.active = false;
        g.game_ptr_down = false; g.press_hit = GameHit{};
        game_menu_pointer(42, 92, true, true);
        game_menu_pointer(42, 92, true, false);
        gcheck(g.plan.active && g.plan.open && g.plan.want.a == 5,
               "Loslassen auf demselben Ordner oeffnet ihn");
        // Das Mod-Menue bricht den Weg ab
        set_menu(1, 0, 0, 0, 0);
        g.game_ptr_down = false; g.press_hit = GameHit{};
        game_menu_pointer(42, 92, true, true);
        menu_opened();
        gcheck(!g.plan.active, "Das Mod-Menue bricht einen laufenden Weg ab");
        g.open = false; g.pending_song = -1;

        // Ausserhalb des Hauptmenues passiert nichts
        bus_write_u16(kCurrentScene, 2);
        gcheck(hit(42, 92).kind == kHitNone, "Ausserhalb des Hauptmenues trifft nichts");
    }
    std::printf("\n  Spielmenue: %d Pruefungen daneben\n", gbad);
    return bad + mbad + gbad;
}

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--selftest") == 0) {
        // Zweites Argument: die ROM. Ohne sie bleiben die Pruefungen gegen
        // die Menuetabellen des Spiels aus.
        if (argc > 2 && !load_rom(argv[2]))
            std::printf("ROM \"%s\" nicht lesbar\n", argv[2]);
        std::printf("Selbsttest der Zeigerrechnung\n\n");
        const int bad = selftest();
        std::printf(bad ? "FEHLGESCHLAGEN\n" : "bestanden\n");
        return bad ? 1 : 0;
    }
    const char* out_path = (argc > 1) ? argv[1] : "ww_mod_menu.ppm";
    const int win_w = (argc > 2) ? std::atoi(argv[2]) : 960;
    const int win_h = (argc > 3) ? std::atoi(argv[3]) : 540;
    const char* game_path = (argc > 4 && argv[4][0] != '-') ? argv[4] : nullptr;
    const int rot = (argc > 5) ? std::atoi(argv[5]) : 0;
    const int cursor = (argc > 6) ? std::atoi(argv[6]) : 0;

    // Spielbild: entweder ein echtes Einzelbild oder ein Streifenmuster.
    int gw = 240, gh = 160;
    std::vector<uint8_t> game;
    if (!game_path || !read_ppm(game_path, &game, &gw, &gh)) {
        gw = 240; gh = 160;
        game.assign(static_cast<size_t>(gw) * gh * 3, 0);
        for (int y = 0; y < gh; ++y)
            for (int x = 0; x < gw; ++x) {
                uint8_t* p = &game[(static_cast<size_t>(y) * gw + x) * 3];
                p[0] = static_cast<uint8_t>(x * 255 / gw);
                p[1] = static_cast<uint8_t>(y * 255 / gh);
                p[2] = static_cast<uint8_t>(((x / 16) + (y / 16)) & 1 ? 220 : 40);
            }
    }

    // Gast auf "Gameplay" stellen, damit die Anzeigezeile Zahlen zeigt.
    bus_write_u16(0x03003844u, 2);              // gCurrentScene
    bus_write_u8 (0x03003848u, 7);              // Stage
    bus_write_u8 (0x03003860u + 0x174u, 2);     // Schwierigkeit
    bus_write_u8 (0x03003860u + 0x27Cu, 137);   // aktuelles Microgame

    g.random_mode = kRandomGames;
    g.seed        = 42;
    g.inf_lives   = true;
    g.speed_pct   = 130;
    g.readout     = true;
    g.music_vol   = 70;
    g.sfx_vol     = 100;
    g.menu_music  = true;
    g.rotation    = rot & 3;
    g.cursor      = cursor;
    g.last_original = 71;
    g.last_swapped  = 137;

    int sw = 0, sh = 0;
    if (!ww_mod_surface_size(win_w, win_h, &sw, &sh)) { std::fprintf(stderr, "keine Flaeche\n"); return 1; }
    std::vector<uint8_t> out(static_cast<size_t>(sw) * sh * 3, 0);

    // Zeiger: erst ohne Menue zeichnen (Fadenkreuz + Ziehstrecke sichtbar),
    // dann das Menue darueber, damit beides im selben Bild steht.
    g.touch_mode = kTouchModeDrag;
    g.ptr_valid = true;
    g.pointer_down = true;
    g.ptr_ox = 96; g.ptr_oy = 96;
    g.ptr_gx = 150; g.ptr_gy = 70;

    g.open = false;
    ww_mod_compose(game.data(), gw, gh, out.data(), sw, sh);
    if (argc > 7 && std::strcmp(argv[7], "nomenu") == 0) {
        std::FILE* f0 = std::fopen(out_path, "wb");
        if (f0) {
            std::fprintf(f0, "P6\n%d %d\n255\n", sw, sh);
            std::fwrite(out.data(), 1, out.size(), f0);
            std::fclose(f0);
            std::printf("geschrieben: %s (%dx%d, nur Zeiger)\n", out_path, sw, sh);
        }
        return 0;
    }
    // "button": geschlossen, Menueknopf sichtbar (Zeiger hat sich geregt).
    if (argc > 7 && std::strcmp(argv[7], "button") == 0) {
        g.ptr_valid = false;
        g.ui.activity = kUiActivityFrames;
        g.open = false;
    } else {
        g.open = true;
        ww_mod_compose(game.data(), gw, gh, out.data(), sw, sh);   // Layout
        // "pressed": rechter Pfeil der gewaehlten Zeile gerade gedrueckt.
        if (argc > 7 && std::strcmp(argv[7], "pressed") == 0) {
            g.ui.down = true;
            g.ui.press_target = kUiArrowR;
            g.ui.press_row = g.cursor;
        }
    }
    ww_mod_compose(game.data(), gw, gh, out.data(), sw, sh);

    std::FILE* f = std::fopen(out_path, "wb");
    if (!f) { std::perror(out_path); return 1; }
    std::fprintf(f, "P6\n%d %d\n255\n", sw, sh);
    std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);
    std::printf("geschrieben: %s (%dx%d, Fenster %dx%d, Drehung %d)\n",
                out_path, sw, sh, win_w, win_h, (rot & 3) * 90);
    return 0;
}
