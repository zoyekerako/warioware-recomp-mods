// ww_mod.cpp -- Mod-Menue, Zufallsmodus, Lautstaerken und Darstellung.
//
// Kein Byte der ROM wird veraendert. Die Mods lesen und schreiben den
// Gastzustand zur Laufzeit (bus_read_*/bus_write_*), fangen Gastfunktionen
// ueber g_runtime_fn_entry_hook ab -- der in jedem rekompilierten
// Funktionsprolog steht -- und rufen in einem Fall eine Gastfunktion selbst
// auf, wie es die Laufzeit fuer IRQ und SWI ohnehin tut.
//
// ── Woher die Adressen stammen ────────────────────────────────────────
//
// gCurrentSceneData liegt als initialisierte Konstante im .data-Bereich der
// ROM (0x083A3D90) und haelt 0x03003860; data/scenes.c schreibt sie einmal
// und niemand danach. Jede Szene legt ihre eigene Struktur ueber diesen
// Puffer -- deshalb ist jeder Dauereingriff auf gCurrentScene == 2
// (GLOBAL_SCENE_GAMEPLAY) beschraenkt, sonst wuerde er fremde Szenendaten
// zerschreiben.
//
// Die Feldabstaende stehen in wariowareinc-main/src/scenes/gameplay.h, die
// Auswahl des naechsten Microgames in asm/scenes/gameplay/asm_08008e1c.s:
//
//   08008E88  ADDS R0,#0x71 / ADDS R0,R0,R5   Reihenfolgetabelle[unk70]
//   08008E90  LDR  R1,[R6,#4]                 Liste des Stages, 8 Byte je Eintrag
//   08008E9C  STRB R1,[R0]  (0x174)           Schwierigkeit = Eintrag.Byte0
//   08008F16  STRB R0,[R1]  (0x27C)           currentMicrogameID = Wort0 >> 8
//   08008F82  STR  R0,[R6,#0xC]               Musik = Eintrag+4, sonst
//                                             D_083A50E0[Spiel].unk4
//   08008FCA  STRB R0,[R1]  (0x172)           Rahmenskript = ...unk8
//   080090EA  LDR  R0,[R0]                    args[1] = D_083A50E0[Spiel].unk0
//   080090FA  BL   set_beatscript_subscenes   R0 = &args[0]
//
// Genau an diesem letzten BL setzt der Zufallsmodus an: R14 == 0x080090FE
// identifiziert die Aufrufstelle eindeutig, R0 zeigt auf {Rahmenskript,
// Microgame-Skript, 0} im Gaststack. Getauscht werden args[1], unk27C und
// -- das war anfangs vergessen -- unkC, die Musik.
//
// ── Warum nur innerhalb der Gruppe getauscht wird ─────────────────────
//
// D_083A50E0[i].unk8 waehlt das Rahmenskript und teilt die 250 Eintraege in
// vier zusammenhaengende Bloecke. Der Klartextname in .unkC verraet, was sie
// sind:
//
//   unk8=1   0..190  (191)  gewoehnliche Microgames  "DODGE WITH JUMP"
//   unk8=2 191..214   (24)  Bossspiele               "L. CUP NOODLE"
//   unk8=4 215..240   (26)  freischaltbare Spiele    "LP. STREET FIGHT"
//   unk8=6 241..249    (9)  Zwischenbilder           "REST WARIO2"
//
// Das sind verschiedene Spielarten, nicht bloss verschiedene Rahmen. Ein
// freischaltbares Spiel in einen Microgame-Platz zu setzen haengt den
// Stage-Ablauf auf -- gemessen: von zwoelftausend Bildern blieben nach dem
// ersten gruppenfremden Tausch keine weiteren Microgame-Starts uebrig,
// waehrend derselbe Lauf gruppentreu einundzwanzig schaffte.
//
// ── Zwischenszenen ueberspringen ──────────────────────────────────────
//
// Das Spiel hat den Sprung eingebaut und sperrt ihn nur beim ersten Mal.
// gameplay_run_script prueft zwei Tore, bevor A/B/Start eine Szene
// abbrechen duerfen: D_030048B8.unk0 Bit 0 (das Skript markiert den
// Abschnitt als ueberspringbar) und gGameplayData.unk6_8 (die Erlaubnis).
// Die Erlaubnis vergibt func_0801720C:
//
//   R0 = func_080007C0(id)      schon gesehen?
//   if (R0 != 0) func_0800A3A4(1)      -> Springen erlaubt
//   else       { func_0800A3A4(0)      -> beim ersten Mal verboten
//                func_08000778(id)      und als gesehen vermerken
//                func_0800A270(); }     und speichern
//
// func_0800A3A4 (0x0800A3A4) ist der einzige Schreiber von unk6_8 ausser
// zwei Stellen in gameplay.c, die es ohnehin auf 1 setzen. Die Funktion
// hat der Funktionsfinder allerdings nicht als eigenen Eintrag erfasst --
// es gibt keinen g_runtime_fn_entry_hook(0x0800A3A4u) im erzeugten Code --,
// also wird das Bit je Bild gesetzt statt der Aufruf abgefangen. Das
// zweite Tor bleibt unangetastet: welcher Abschnitt ueberspringbar ist,
// entscheidet weiter das Skript.

// ── KaizoWare und Boss Rush ───────────────────────────────────────────
//
// Schwierigkeit: gGameplayData.currentDifficulty (+0x174) wird in
// func_08008E1C aus Byte 0 des Stage-Eintrags gesetzt und von 35 Microgames
// ueber func_0800A024 gelesen. Der Bereich steht in func_080A190C: der Index
// wird als (Sprache * 3 + Stufe) * 4 gebildet, also Dreiergruppen -- Stufen
// 0, 1, 2, und 2 ist die haerteste.
//
// Boss: gGameplayData.unk6c zeigt auf struct GameplayStruct6c, dessen
// unk0_1 (Bits 0..7 des ersten Worts) sagt, wie viele Microgames vor dem
// Boss kommen. gameplay_run_script Fall 0 endet mit
//
//     if (gGameplayData.unk70 < scriptTarget->unk0_1) { func_08008E1C(); return 0; }
//
// -- unk70 auf die Schwelle zu heben laesst die Stage also nach dem naechsten
// abgeschlossenen Microgame zum Boss springen, ohne dass func_08008E1C noch
// einmal in die Reihenfolgetabelle greift.

// ── Ton ───────────────────────────────────────────────────────────────
//
// sound_player_table (0x08406430) haelt neun Zeiger auf SoundPlayer-
// Strukturen im IWRAM, sound_player_count (0x0840642C) sagt neun. Ob gerade
// Musik oder ein Effekt laeuft, verraet der Klartextname des geladenen
// Stuecks (SoundPlayer.song +0x0C -> SongHeader.title +0x0C):
// "m_BGM_..." gegen "s_BASIC_...".
//
// Welches Feld den Pegel bestimmt, war nicht zu erraten. set_soundplayer_volume
// (0x080020C8) reicht an func_080F2F64 weiter, und das ist eine einzige
// Anweisung: STRH R2,[R0,#0x26] -- aber dieses Feld auf ein Viertel zu setzen
// senkte den gemessenen Pegel nur um 12 %. Es ist der *Sollwert*; der Treiber
// rechnet daraus seinen eigenen. Gefunden wurde das richtige Feld durch
// Messen: WWMOD_MUTE_OFFSET nullt in jedem Bild ein u16 an einem waehlbaren
// Abstand, und ein Durchlauf ueber 0x1E..0x2C zeigte, welcher greift:
//
//   Abstand   0x1E   0x20   0x22   0x24   0x26   0x28   0x2A   0x2C
//   Restpegel  100 %  100 %   50 %  100 %   88 %  1,6 %  100 %   99 %
//
// 0x28 ist also der wirksame Pegel. Die Sonde bleibt eingebaut -- sie hat
// die Frage einmal beantwortet und wuerde es wieder tun.

#include "ww_mod.h"
#include "ww_font.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "runtime_arm.h"  // g_cpu, g_runtime_fn_entry_hook, bus_*, runtime_dispatch
#include "presentation_layout.h"  // dieselbe Rechnung, mit der das Fenster zeigt

#ifdef GBARECOMP_HAVE_SDL2
#include <SDL.h>
#endif

// Zaehler der VBlank-Anfaenge (runtime_bus_bridge). Er gibt den Takt des
// Gastes an, nicht den des Fensters -- Dauerwirkungen haengen daran und
// gelten deshalb auch in einem Lauf ohne Fenster.
extern "C" unsigned long long g_runtime_vblank_starts;

namespace {

// ── Gastadressen ──────────────────────────────────────────────────────
constexpr uint32_t kSceneData      = 0x03003860u;  // gCurrentSceneData-Wert
constexpr uint32_t kCurrentScene   = 0x03003844u;  // s16, 2 = Gameplay
constexpr uint32_t kStageId        = 0x03003848u;  // u8
constexpr uint32_t kMicrogameTable = 0x083A50E0u;  // 16 Byte je Eintrag
constexpr int      kMicrogameCount = 250;
constexpr int      kSceneGameplay  = 2;

// struct GameplayData (gameplay.h)
constexpr uint32_t kOffData0       = 0x00u;   // unk0
constexpr uint32_t kOffStateWord   = 0x04u;   // Bitfeld, Bits 0..4 = currentState
constexpr uint32_t kBitSkipAllowed = 1u << 23;  // unk6_8, gameplay.h "0x4:23"
constexpr uint32_t kOffMusic       = 0x0Cu;   // struct SongHeader* unkC
constexpr uint32_t kOffScriptTarget = 0x6Cu;  // struct GameplayStruct6c* unk6c
constexpr uint32_t kOffGamesPlayed = 0x70u;   // u8  unk70
constexpr uint32_t kOffWrapper     = 0x172u;  // u8  unk172
constexpr uint32_t kOffDifficulty  = 0x174u;  // u8, 0..2
constexpr uint8_t  kMaxDifficulty  = 2;
constexpr uint32_t kOffLives       = 0x175u;  // u8
constexpr uint32_t kOffMaxLives    = 0x176u;  // u8
constexpr uint32_t kOffScore       = 0x17Cu;  // u16
constexpr uint32_t kOffMusicMode   = 0x228u;  // u32 unk228
constexpr uint32_t kOffMicrogameId = 0x27Cu;  // u8

constexpr int kStateRunning = 1;
constexpr int kStatePaused  = 4;

// Abgefangene und gerufene Gastfunktionen
constexpr uint32_t kFnSetSubscenes = 0x0800986Cu;  // set_beatscript_subscenes
constexpr uint32_t kFnSetTempo     = 0x08009E20u;  // set_beatscript_tempo
constexpr uint32_t kFnSongStart    = 0x080F2E48u;  // func_080F2E48(u16 songId)
// Ruecksprungadresse des BL bei 0x080090FA. Der Erzeuger legt R14 ohne
// Thumb-Bit ab (recompiled_000.cpp: g_cpu.R[14] = 0x080090FEu).
constexpr uint32_t kRetFromStart   = 0x080090FEu;

// Ton
constexpr uint32_t kSoundPlayerCount = 0x0840642Cu;  // u8
constexpr uint32_t kSoundPlayerTable = 0x08406430u;  // 12 Byte je Eintrag
constexpr uint32_t kOffPlayerSong    = 0x0Cu;        // SoundPlayer.song
constexpr uint32_t kOffPlayerTarget  = 0x26u;        // Sollwert, ROM-belegt
constexpr uint32_t kOffPlayerVolume  = 0x28u;        // wirksamer Pegel, gemessen
constexpr uint32_t kOffSongTitle     = 0x0Cu;        // SongHeader.title
constexpr uint32_t kOffSongNum       = 0x10u;        // SongHeader.songNum
constexpr uint16_t kSongTitleTheme   = 10;           // m_BGM_Title_01
constexpr int      kMaxSoundPlayers  = 16;

inline bool is_rom(uint32_t a) { return a >= 0x08000000u && a < 0x08800000u; }
inline bool is_ram(uint32_t a) { return a >= 0x02000000u && a < 0x08000000u; }

// ── Zustand ───────────────────────────────────────────────────────────
enum RandomMode { kRandomOff = 0, kRandomGames = 1, kRandomAll = 2 };
constexpr uint8_t kWrapperMicrogame = 1;

enum ScreenMode { kScreenSharp = 0, kScreenFit = 1, kScreenStretch = 2 };

// ── Zeigersteuerung ───────────────────────────────────────────────────
// WarioWare Inc. kennt keinen Cursor: jedes Microgame liest gPressedKeys
// bzw. gCurrentKeys. Ein Zeiger kann also nur Tastendruecke *erfinden*, und
// die einzige offene Frage ist, welche. Vier Bausteine:
//
//   tap    Aufsetzen = A, kurzer Impuls. Fuer "im richtigen Moment druecken".
//   hold   A gedrueckt, solange der Zeiger aufliegt. Fuer Spiele, die den
//          Knopf halten oder aufladen lassen -- der Fernlenk-Staubsauger in
//          CHIRITORIE lenkt genau so.
//   drag   Ziehen = gehaltene Richtung, sobald die Totzone verlassen ist.
//   flick  Wischen = eine einzelne Flanke. Fuer Spiele, die gPressedKeys
//          (Flanke) lesen statt gCurrentKeys (Pegel).
//   both   tap + drag, die Voreinstellung.
//
// Welches ein Microgame braucht, steht in seinem Code und ist von aussen
// nicht rechtzeitig erkennbar -- deshalb gibt es eine Tabelle, ein Schema je
// Microgame-Nummer, ueber WWMOD_TOUCH_TABLE nachladbar. Leer heisst: alle
// bekommen die Voreinstellung.
enum TouchScheme { kTouchInherit = 0, kTouchOff, kTouchTap, kTouchHold,
                   kTouchDrag, kTouchBoth, kTouchFlick };
enum TouchMode { kTouchModeOff = 0, kTouchModeDrag, kTouchModeFlick };

// Der Gast pollt einmal je Bild; ein erfundener Druck muss lange genug
// stehen und danach wieder losgelassen werden, sonst kommt keine Flanke an.
constexpr int kTouchPressPumps = 3;
constexpr int kTouchDeadZone   = 8;    // Spielpunkte, bis Ziehen greift
constexpr int kTouchFlickDist  = 14;   // Spielpunkte fuer ein Wischen
enum BackdropMode { kBackdropAmbient = 0, kBackdropBlack = 1 };

enum ItemId {
    kItemRandom = 0, kItemSeed, kItemKaizo, kItemBossRush,
    kItemLives, kItemSkip, kItemSpeed, kItemReadout, kItemTouch,
    kItemMusicVol, kItemSfxVol, kItemMenuMusic,
    kItemWide, kItemScreen, kItemRotation, kItemBackdrop,
    kItemCount
};

// Echtes Breitbild. Die Ansichtshoehe bleibt immer bei 160 Zeilen, also
// bestimmt allein die Breite das Seitenverhaeltnis: 16:9 sind 160*16/9 =
// 284,4 -> 284 Punkte, 16:10 sind 256, 21:9 sind 373,3 -> 374 (gerade, damit
// die Raender links und rechts gleich ausfallen). 480 ist die Grenze der
// Laufzeit (GbaPpu::kMaxRenderWidth).
//
// kWideAuto folgt dem Fensterformat und traegt deshalb keine feste Zahl --
// dieselbe Rechnung, die gbarecomps adaptive Ansicht benutzt.
enum WideMode { kWideOff = 0, kWide16x10, kWide16x9, kWide21x9, kWideMax,
                kWideAuto, kWideCount };
constexpr int kWideSteps[kWideCount] = {240, 256, 284, 374, 480, 0};
constexpr const char* kWideLabels[kWideCount] =
    {"Off", "16:10", "16:9", "21:9", "Max", "Auto"};
// Nach diesen Eintraegen eine Trennlinie ziehen.
// Trennlinien: Modi | Hilfen | Ton | Bild.
constexpr int kRuleAfterA = kItemBossRush;
constexpr int kRuleAfterB = kItemTouch;
constexpr int kRuleAfterC = kItemMenuMusic;

// KaizoWare gibt dem Tempo einen Boden -- der Regler darf hoeher, nicht tiefer.
constexpr int kKaizoSpeedFloor = 200;

struct VolShadow {
    uint16_t base = 0;
    uint16_t written = 0;
    bool     known = false;
};

// ── Menue per Zeiger ──────────────────────────────────────────────────
// Was ein Druck trifft, entscheidet der Moment des Aufsetzens.
enum UiTarget { kUiNone = 0, kUiGame, kUiMenuButton, kUiRow, kUiValue,
                kUiArrowL, kUiArrowR, kUiClose, kUiOutside, kUiPanel };

// Der Menueknopf erscheint, sobald sich der Zeiger regt, und verschwindet
// nach drei Sekunden Ruhe -- das Muster der Bedienleiste eines Videospielers.
constexpr int kUiActivityFrames = 180;

struct UiState {
    bool down = false;
    bool valid = false;
    int  sx = 0, sy = 0;            // Zeiger in Flaechenkoordinaten
    int  last_wx = -1, last_wy = -1;
    int  activity = 0;
    int  press_target = kUiNone;
    int  press_row = -1;
    int  hold = 0, repeats = 0;
};

// Trefferflaechen aus dem letzten gezeichneten Menue. Zeichnen und Antippen
// benutzen dieselben Zahlen -- sonst trifft der Finger daneben, sobald sich
// ein Abstand aendert.
struct MenuLayout {
    bool valid = false;
    int  sc = 1, px = 0, py = 0, pw = 0, ph = 0, row_h = 0;
    int  row_top[kItemCount] = {};
    int  val_x0[kItemCount] = {}, val_x1[kItemCount] = {};
    int  arl_x0[kItemCount] = {};   // linke Pfeilzone reicht von hier bis val_x0
    int  close_x0 = 0, close_y0 = 0, close_x1 = 0, close_y1 = 0;
};

struct ButtonRect { bool visible = false; int x0 = 0, y0 = 0, x1 = 0, y1 = 0; };

// ── Die Menues des Spiels per Zeiger ──────────────────────────────────
// Was ein Klick im Hauptmenue des Spiels getroffen hat, und der Plan, der
// die Hand dorthin bringt. Beides steht hier oben, weil ModState es haelt.
enum HitKind { kHitNone = 0, kHitTab, kHitBack, kHitFolder, kHitCell,
               kHitOption, kHitDialog, kHitViewer };

struct GameHit {
    int kind = kHitNone;
    int a = 0, b = 0;   // Reiter / Ordner / Spalte+Zeile, je nach kind
};
inline bool same_hit(const GameHit& x, const GameHit& y) {
    return x.kind == y.kind && x.a == y.a && x.b == y.b;
}

struct GamePlan {
    bool    active = false;
    GameHit want;
    bool    open   = false;   // am Ziel A druecken
    int     budget = 0;       // Restbilder, damit nichts ewig laeuft
    bool    snap   = false;   // Zelle aus der Vorschau: erst im Gitter pruefen
    int     px = 0, py = 0;   // wo der Druck aufsetzte
};

struct ModState {
    bool open   = false;
    int  cursor = 0;

    int  random_mode = kRandomOff;
    int  seed        = 1;
    bool inf_lives   = false;
    bool skip_scenes = false;
    bool kaizo       = false;
    bool boss_rush   = false;
    int  speed_pct   = 100;
    bool readout     = false;

    int  music_vol   = 100;
    int  sfx_vol     = 100;
    bool menu_music  = true;

    int  wide        = kWideOff;
    int  win_w = 0, win_h = 0;   // zuletzt gemeldete Fenstergroesse

    // Zeigersteuerung
    int  touch_mode = kTouchModeOff;
    unsigned char touch_table[kMicrogameCount] = {};  // kTouchInherit = Vorgabe
    bool pointer_down = false, pointer_was = false;
    int  ptr_gx = 0, ptr_gy = 0;        // Zeiger in Spielkoordinaten
    int  ptr_ox = 0, ptr_oy = 0;        // Aufsetzpunkt
    bool ptr_valid = false;
    int  touch_press_left = 0;          // Restbilder des A-Impulses
    int  touch_dir_left = 0;            // Restbilder eines Wisch-Impulses
    uint16_t touch_dir_bits = 0;
    bool flick_fired = false;
    uint16_t touch_out = 0;             // was zuletzt eingespeist wurde

    // Bildgeometrie der letzten Zusammensetzung, fuer die Umkehrrechnung
    int geo_sw = 0, geo_sh = 0;         // Zeichenflaeche
    int geo_dx = 0, geo_dy = 0, geo_dw = 0, geo_dh = 0;  // Spielbild darin
    int geo_gw = 240, geo_gh = 160, geo_rot = 0;

    // Menue per Zeiger
    UiState    ui;
    MenuLayout ml;
    ButtonRect mbtn;

    // Menues des Spiels per Zeiger
    GamePlan plan;
    GameHit  press_hit;            // was der Druck im Spielmenue getroffen hat
    bool     game_ptr_down = false;
    int      last_menu_key = 0;    // nur fuer die Spur
    int  screen      = kScreenSharp;
    int  rotation    = 0;     // 0..3, je 90 Grad
    int  backdrop    = kBackdropAmbient;

    // Bild
    std::vector<uint8_t> frozen;      // eingefrorenes Spielbild hinter dem Menue
    int frozen_w = 0, frozen_h = 0;
    std::vector<uint8_t> ambient;     // zwischengespeicherter Hintergrund
    int ambient_w = 0, ambient_h = 0;
    unsigned ambient_age = 0;

    // Eingabe
    uint16_t prev_keys   = 0x03FFu;
    int      hold_frames = 0;
    int      held_dir    = 0;
    bool     prev_f10    = false;
    uint16_t inject_bits = 0;
    int      inject_left = 0;

    // Takt und aufgeschobene Gastaufrufe
    unsigned long long last_vblank = ~0ull;
    int      pending_song = -1;      // >=0: dieses Stueck anstossen
    int      saved_song   = -1;      // was vor dem Menue lief
    bool     guest_call_ok = true;

    // Ton
    VolShadow vol[kMaxSoundPlayers];
    int       player_count = 0;

    // Anzeige
    int      last_original = -1;
    int      last_swapped  = -1;
    uint64_t swaps         = 0;
    uint64_t skips_granted = 0;
    bool     skip_seen_set = false;
};

ModState g;
void (*g_prev_entry_hook)(uint32_t) = nullptr;

// ── Gastspeicher ──────────────────────────────────────────────────────
inline bool in_gameplay() {
    return static_cast<int16_t>(bus_read_u16(kCurrentScene)) == kSceneGameplay;
}
inline uint32_t gp(uint32_t off) { return kSceneData + off; }
inline int gameplay_state() {
    return static_cast<int>(bus_read_u32(gp(kOffStateWord)) & 0x1Fu);
}
inline uint32_t microgame_field32(int idx, uint32_t off) {
    return bus_read_u32(kMicrogameTable + static_cast<uint32_t>(idx) * 16u + off);
}
inline uint8_t microgame_wrapper(int idx) {
    return bus_read_u8(kMicrogameTable + static_cast<uint32_t>(idx) * 16u + 8u);
}

// Gruppengrenzen zu einem Rahmenskript-Wert. Linear gesucht statt fest
// verdrahtet, damit die Tabelle die Wahrheit bleibt.
void group_bounds(uint8_t wrapper, int* first, int* count) {
    int f = -1, n = 0;
    for (int i = 0; i < kMicrogameCount; ++i) {
        if (microgame_wrapper(i) == wrapper) {
            if (f < 0) f = i;
            ++n;
        } else if (f >= 0) {
            break;  // Gruppen liegen zusammenhaengend
        }
    }
    *first = (f < 0) ? 0 : f;
    *count = (n <= 0) ? kMicrogameCount : n;
}

// splitmix32 -- reine Funktion, damit ein Wiedereintritt derselben
// Gastfunktion (runtime_should_yield laesst sie von vorn beginnen)
// dieselbe Wahl trifft und der Eingriff idempotent bleibt.
uint32_t mix32(uint32_t x) {
    x += 0x9E3779B9u;
    x = (x ^ (x >> 16)) * 0x85EBCA6Bu;
    x = (x ^ (x >> 13)) * 0xC2B2AE35u;
    return x ^ (x >> 16);
}
uint32_t decide(uint32_t seed, uint32_t stage, uint32_t played, uint32_t score) {
    return mix32(mix32(mix32(mix32(seed) ^ stage) ^ played) ^ score);
}

// ── Einen Gastaufruf ausfuehren ───────────────────────────────────────
// Dasselbe Verfahren, mit dem die Laufzeit IRQ und SWI ausfuehrt: Register
// sichern, Ruecksprungmarke auf den Aufrufstapel legen, dispatchen, Register
// zuruecklegen. Nur aus ww_mod_frame() gerufen -- also aus dem
// Present-Haken, und der laeuft erst, nachdem runtime_should_yield seine
// VBlank-Marke schon verbraucht hat (g_runtime_yielded_vblank_start wird vor
// dem Haken gesetzt). Ein sofortiges Zurueckfallen aus der verschachtelten
// Ausfuehrung ist damit ausgeschlossen.
bool call_guest(uint32_t fn_thumb, uint32_t a0) {
    if (!g.guest_call_ok) return false;
    const ArmCpuState save = g_cpu;
    const uint32_t ret = 0x08000000u;   // beliebige Marke; nur der Abgleich zaehlt
    g_cpu.R[0]  = a0;
    g_cpu.R[14] = ret | 1u;
    g_cpu.R[15] = fn_thumb;
    g_cpu.cpsr |= CPSR_T_BIT;
    runtime_call_push_return(ret);
    runtime_dispatch(fn_thumb);
    const bool ok = ((g_cpu.R[15] & ~1u) == ret);
    if (!ok) runtime_call_cancel_return(ret);
    g_cpu = save;
    return ok;
}

// ── Zufallsmodus ──────────────────────────────────────────────────────
void apply_randomizer() {
    if (g.random_mode == kRandomOff || !in_gameplay()) return;

    const uint32_t args = g_cpu.R[0];      // &args[0] im Gaststack
    if (!is_ram(args)) return;

    const int original = bus_read_u8(gp(kOffMicrogameId));
    if (original < 0 || original >= kMicrogameCount) return;

    // unk172 traegt bereits D_083A50E0[Original].unk8 -- die Gruppe, in der
    // getauscht werden darf. Im Modus "Microgames" bleibt alles andere
    // unberuehrt.
    const uint8_t wrapper = bus_read_u8(gp(kOffWrapper));
    if (g.random_mode == kRandomGames && wrapper != kWrapperMicrogame) return;

    const uint32_t h = decide(static_cast<uint32_t>(g.seed),
                              bus_read_u8(kStageId),
                              bus_read_u8(gp(kOffGamesPlayed)),
                              bus_read_u16(gp(kOffScore)));

    int first = 0, count = kMicrogameCount;
    group_bounds(wrapper, &first, &count);
    const int chosen = first + static_cast<int>(h % static_cast<uint32_t>(count));
    if (chosen < 0 || chosen >= kMicrogameCount) return;

    const uint32_t script = microgame_field32(chosen, 0u);
    if (!is_rom(script)) return;

    // args[1] = Skript des gewaehlten Microgames, unk27C = seine Nummer.
    // Rahmenskript (args[0]) und unk172 bleiben stehen: der Ersatz stammt
    // aus derselben Gruppe und braucht denselben Rahmen.
    bus_write_u32(args + 4u, script);
    bus_write_u8(gp(kOffMicrogameId), static_cast<uint8_t>(chosen));

    // Die Musik gehoert zum Microgame, nicht zum Platz. func_08008E1C hat
    // unkC schon gesetzt -- aber fuer das urspruengliche Spiel. Nur bei
    // unk228 == 0 stammt sie ueberhaupt aus dem Eintrag bzw. aus
    // D_083A50E0[Spiel].unk4; bei 1 und 2 kommt sie aus einer Auswahlliste
    // (Stage-Medley), und die bleibt unangetastet.
    if (bus_read_u32(gp(kOffMusicMode)) == 0u) {
        const uint32_t song = microgame_field32(chosen, 4u);
        if (is_rom(song)) bus_write_u32(gp(kOffMusic), song);
    }

    if (chosen != g.last_swapped || original != g.last_original) ++g.swaps;
    g.last_original = original;
    g.last_swapped  = chosen;

    // WWMOD_TRACE=1 belegt, dass der Tausch greift, ohne dass jemand
    // zuschauen muss -- eine Zeile je Microgame-Start.
    static const bool trace = std::getenv("WWMOD_TRACE") != nullptr;
    if (trace) {
        const uint32_t song = bus_read_u32(gp(kOffMusic));
        uint32_t num = 0;
        if (is_rom(song)) num = bus_read_u32(song + kOffSongNum);
        std::fprintf(stderr,
                     "[ww-mod] Stage %2u Spiel %3u: %3d -> %3d "
                     "(Rahmen %u, Skript %08X, Musik %4u, Stufe %u, "
                     "Leben %u/%u)\n",
                     bus_read_u8(kStageId), bus_read_u8(gp(kOffGamesPlayed)),
                     original, chosen, wrapper, script, num,
                     bus_read_u8(gp(kOffDifficulty)),
                     bus_read_u8(gp(kOffLives)), bus_read_u8(gp(kOffMaxLives)));
    }
}

// ── Tempo ─────────────────────────────────────────────────────────────
void apply_tempo() {
    const int pct = (g.kaizo && g.speed_pct < kKaizoSpeedFloor)
                        ? kKaizoSpeedFloor : g.speed_pct;
    if (pct == 100 || !in_gameplay()) return;
    // set_beatscript_tempo(u16). Der Deckel aus unk280 wirkt beim Aufrufer,
    // hier ist er bereits passiert -- der Faktor gilt also unbeschnitten.
    uint32_t t = g_cpu.R[0] & 0xFFFFu;
    if (t == 0) return;
    t = t * static_cast<uint32_t>(pct) / 100u;
    if (t < 20u)  t = 20u;
    if (t > 600u) t = 600u;
    g_cpu.R[0] = t;
}

// ── Dauerwirkungen, einmal je Gastbild ────────────────────────────────
void apply_volumes() {
    if (g.music_vol == 100 && g.sfx_vol == 100) {
        // Nichts zu tun -- aber den Schattenwert nachfuehren, damit ein
        // spaeteres Herunterregeln nicht den alten Grundwert erwischt.
        for (int i = 0; i < g.player_count; ++i) g.vol[i].known = false;
        return;
    }
    for (int i = 0; i < g.player_count; ++i) {
        const uint32_t sp = bus_read_u32(kSoundPlayerTable +
                                         static_cast<uint32_t>(i) * 12u);
        if (!is_ram(sp)) continue;
        const uint16_t cur = bus_read_u16(sp + kOffPlayerVolume);
        VolShadow& s = g.vol[i];
        // Alles, was nicht von uns stammt, ist der neue Grundwert -- so
        // ueberleben die Ein- und Ausblendungen des Spiels.
        if (!s.known || cur != s.written) { s.base = cur; s.known = true; }

        int pct = g.music_vol;
        const uint32_t song = bus_read_u32(sp + kOffPlayerSong);
        if (is_rom(song)) {
            const uint32_t title = bus_read_u32(song + kOffSongTitle);
            if (is_rom(title) && bus_read_u8(title) == 's') pct = g.sfx_vol;
        }
        uint32_t tgt = static_cast<uint32_t>(s.base) * static_cast<uint32_t>(pct) / 100u;
        if (tgt > 0xFFFFu) tgt = 0xFFFFu;
        if (cur != static_cast<uint16_t>(tgt))
            bus_write_u16(sp + kOffPlayerVolume, static_cast<uint16_t>(tgt));
        s.written = static_cast<uint16_t>(tgt);
        static const bool vtrace = std::getenv("WWMOD_VOL_TRACE") != nullptr;
        if (vtrace) {
            static unsigned long n = 0;
            if ((n++ % (60u * 9u)) < 9u) {
                uint32_t num = 0; char c = '?';
                if (is_rom(song)) {
                    num = bus_read_u32(song + kOffSongNum);
                    const uint32_t t2 = bus_read_u32(song + kOffSongTitle);
                    if (is_rom(t2)) c = static_cast<char>(bus_read_u8(t2));
                }
                std::fprintf(stderr, "[vol] Spieler %d @%08X Stueck %4u '%c' "
                             "grund=%u ist=%u ziel=%u (%d%%)\n",
                             i, sp, num, c, s.base, cur, tgt, pct);
            }
        }
    }
}

// Suchhilfe: WWMOD_MUTE_OFFSET=<hex> nullt bei jedem Bild das u16 an diesem
// Abstand in allen Sound-Spielern. Damit laesst sich das Feld, das den Pegel
// wirklich bestimmt, messend finden statt raten -- welcher Abstand die
// Ausgabe verstummen laesst, ist der gesuchte.
void mute_probe() {
    static const long off = [] {
        const char* e = std::getenv("WWMOD_MUTE_OFFSET");
        return e ? std::strtol(e, nullptr, 16) : -1L;
    }();
    if (off < 0) return;
    for (int i = 0; i < g.player_count; ++i) {
        const uint32_t sp = bus_read_u32(kSoundPlayerTable + static_cast<uint32_t>(i) * 12u);
        if (is_ram(sp)) bus_write_u16(sp + static_cast<uint32_t>(off), 0);
    }
}

void tick_effects() {
    mute_probe();
    if (g.skip_scenes && in_gameplay()) {
        // Nur die Erlaubnis setzen, nichts erzwingen: A/B/Start brechen die
        // Szene dann genauso ab wie beim zweiten Durchgang.
        const uint32_t w = bus_read_u32(gp(kOffStateWord));
        if (!(w & kBitSkipAllowed)) {
            // Beim Szenenwechsel ist das ganze Wort kurz genullt, bevor
            // gameplay_stage_init unk6_8 auf 1 setzt. Nur wenn das Bit
            // vorher schon einmal gesetzt *gesehen* wurde, hat das Spiel es
            // aktiv geloescht -- und genau das ist die Erstspiel-Sperre.
            if (g.skip_seen_set) {
                ++g.skips_granted;
                static const bool trace = std::getenv("WWMOD_TRACE") != nullptr;
                if (trace)
                    std::fprintf(stderr,
                                 "[ww-mod] Sperre aufgehoben (Stage %u, "
                                 "Zustand %u, Nr. %llu)\n",
                                 bus_read_u8(kStageId), w & 0x1Fu,
                                 (unsigned long long)g.skips_granted);
            }
            bus_write_u32(gp(kOffStateWord), w | kBitSkipAllowed);
        } else {
            g.skip_seen_set = true;
        }
    } else if (!in_gameplay()) {
        g.skip_seen_set = false;
    }
    if (g.kaizo && in_gameplay()) {
        // Haerteste Stufe ab dem ersten Microgame. func_08008E1C setzt das
        // Feld bei jedem Start neu aus dem Stage-Eintrag, deshalb wird es
        // je Bild nachgezogen statt einmal.
        const uint8_t cur = bus_read_u8(gp(kOffDifficulty));
        if (cur != kMaxDifficulty) {
            bus_write_u8(gp(kOffDifficulty), kMaxDifficulty);
            static const bool trace = std::getenv("WWMOD_TRACE") != nullptr;
            if (trace)
                std::fprintf(stderr, "[ww-mod] Stufe %u -> %u angehoben\n",
                             cur, kMaxDifficulty);
        }
    }
    if (g.boss_rush && in_gameplay()) {
        // unk70 auf die Bossschwelle heben. Nur wenn der Zeiger plausibel
        // ist und die Schwelle einen sinnvollen Wert hat -- ausserhalb der
        // Microgame-Folge steht dort nichts Brauchbares.
        const uint32_t target = bus_read_u32(gp(kOffScriptTarget));
        if (is_ram(target) || is_rom(target)) {
            const uint32_t threshold = bus_read_u32(target) & 0xFFu;
            if (threshold >= 1u && threshold <= 0xFEu &&
                bus_read_u8(gp(kOffGamesPlayed)) < threshold) {
                bus_write_u8(gp(kOffGamesPlayed), static_cast<uint8_t>(threshold));
            }
        }
    }
    if (g.inf_lives && in_gameplay()) {
        // currentLives-- und die Pruefung auf 0 stehen im Spiel unmittelbar
        // beieinander (gameplay_run_script, Fall 0). Ein Auffuellen je Bild
        // greift deshalb rechtzeitig: aus 4-- wird 3, nie 0.
        const uint8_t maxl = bus_read_u8(gp(kOffMaxLives));
        if (maxl > 0 && maxl <= 8 && bus_read_u8(gp(kOffLives)) != maxl)
            bus_write_u8(gp(kOffLives), maxl);
    }
    apply_volumes();
}

inline bool effects_armed() {
    static const bool probe = std::getenv("WWMOD_MUTE_OFFSET") != nullptr;
    return probe || g.inf_lives || g.skip_scenes || g.kaizo || g.boss_rush ||
           g.music_vol != 100 || g.sfx_vol != 100;
}

void entry_hook(uint32_t pc) {
    if (effects_armed() && g_runtime_vblank_starts != g.last_vblank) {
        g.last_vblank = g_runtime_vblank_starts;
        tick_effects();
    }
    if (pc == kFnSetSubscenes) {
        const uint32_t lr = g_cpu.R[14];
        if (lr == kRetFromStart || lr == (kRetFromStart | 1u)) apply_randomizer();
    } else if (pc == kFnSetTempo) {
        apply_tempo();
    }
    if (g_prev_entry_hook) g_prev_entry_hook(pc);
}

// ── Zeichnen ──────────────────────────────────────────────────────────
struct Rgb { uint8_t r, g, b; };

constexpr Rgb kColText  = {236, 236, 232};
constexpr Rgb kColDim   = {150, 150, 158};
constexpr Rgb kColPick  = {255, 214,  64};
constexpr Rgb kColTitle = {255, 236, 120};
constexpr Rgb kColValue = {130, 226, 255};
constexpr Rgb kColRule  = { 92,  92, 116};

inline void put_px(uint8_t* fb, int w, int h, int x, int y, Rgb c) {
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    uint8_t* p = fb + (static_cast<size_t>(y) * w + x) * 3;
    p[0] = c.r; p[1] = c.g; p[2] = c.b;
}

void fill_rect(uint8_t* fb, int w, int h, int x0, int y0, int rw, int rh,
               Rgb c, int alpha_pct) {
    if (x0 < 0) { rw += x0; x0 = 0; }
    if (y0 < 0) { rh += y0; y0 = 0; }
    if (x0 + rw > w) rw = w - x0;
    if (y0 + rh > h) rh = h - y0;
    if (rw <= 0 || rh <= 0) return;
    const int inv = 100 - alpha_pct;
    for (int y = y0; y < y0 + rh; ++y) {
        uint8_t* p = fb + (static_cast<size_t>(y) * w + x0) * 3;
        for (int x = 0; x < rw; ++x, p += 3) {
            if (alpha_pct >= 100) { p[0] = c.r; p[1] = c.g; p[2] = c.b; continue; }
            p[0] = static_cast<uint8_t>((p[0] * inv + c.r * alpha_pct) / 100);
            p[1] = static_cast<uint8_t>((p[1] * inv + c.g * alpha_pct) / 100);
            p[2] = static_cast<uint8_t>((p[2] * inv + c.b * alpha_pct) / 100);
        }
    }
}

int text_width(const char* s, int sc) {
    return static_cast<int>(std::strlen(s)) * (WW_FONT_W + 1) * sc;
}

void draw_text(uint8_t* fb, int w, int h, int x, int y, const char* s,
               Rgb c, int sc) {
    const int adv = (WW_FONT_W + 1) * sc;
    for (; *s; ++s, x += adv) {
        const unsigned ch = static_cast<unsigned char>(*s);
        if (ch < WW_FONT_FIRST || ch > WW_FONT_LAST) continue;
        const unsigned char* glyph = kWwFont[ch - WW_FONT_FIRST];
        for (int gx = 0; gx < WW_FONT_W; ++gx) {
            const unsigned col = glyph[gx];
            if (!col) continue;
            for (int gy = 0; gy < WW_FONT_H; ++gy) {
                if (!((col >> gy) & 1u)) continue;
                if (sc == 1) { put_px(fb, w, h, x + gx, y + gy, c); continue; }
                fill_rect(fb, w, h, x + gx * sc, y + gy * sc, sc, sc, c, 100);
            }
        }
    }
}

void draw_text_shadow(uint8_t* fb, int w, int h, int x, int y, const char* s,
                      Rgb c, int sc) {
    draw_text(fb, w, h, x + sc, y + sc, s, Rgb{0, 0, 0}, sc);
    draw_text(fb, w, h, x, y, s, c, sc);
}

// ── Hintergrund ───────────────────────────────────────────────────────
// Das Spielbild auf ein grobes Raster mitteln, das Raster bilinear auf die
// ganze Flaeche ziehen und abdunkeln. Weil es unscharf ist, faellt die
// niedrige Aufloesung nicht auf -- und weil es sich langsam aendert, wird es
// nur alle paar Bilder neu gerechnet.
constexpr int kAmbW = 12, kAmbH = 8;
constexpr unsigned kAmbInterval = 4;

void build_ambient(const uint8_t* game, int gw, int gh, uint8_t* out, int ow, int oh) {
    uint32_t acc[kAmbH][kAmbW][3] = {};
    uint32_t cnt[kAmbH][kAmbW] = {};
    for (int y = 0; y < gh; ++y) {
        const int cy = y * kAmbH / gh;
        const uint8_t* row = game + static_cast<size_t>(y) * gw * 3;
        for (int x = 0; x < gw; ++x) {
            const int cx = x * kAmbW / gw;
            acc[cy][cx][0] += row[x * 3 + 0];
            acc[cy][cx][1] += row[x * 3 + 1];
            acc[cy][cx][2] += row[x * 3 + 2];
            ++cnt[cy][cx];
        }
    }
    uint8_t grid[kAmbH][kAmbW][3];
    for (int y = 0; y < kAmbH; ++y)
        for (int x = 0; x < kAmbW; ++x) {
            const uint32_t n = cnt[y][x] ? cnt[y][x] : 1u;
            for (int c = 0; c < 3; ++c)
                grid[y][x][c] = static_cast<uint8_t>(acc[y][x][c] / n);
        }
    // Ein 3x3-Kasten darueber nimmt dem Raster die Kanten.
    uint8_t soft[kAmbH][kAmbW][3];
    for (int y = 0; y < kAmbH; ++y)
        for (int x = 0; x < kAmbW; ++x)
            for (int c = 0; c < 3; ++c) {
                uint32_t sum = 0, n = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int yy = y + dy, xx = x + dx;
                        if (yy < 0 || xx < 0 || yy >= kAmbH || xx >= kAmbW) continue;
                        sum += grid[yy][xx][c]; ++n;
                    }
                soft[y][x][c] = static_cast<uint8_t>(sum / (n ? n : 1u));
            }

    for (int y = 0; y < oh; ++y) {
        const int fy = (y * (kAmbH - 1) * 256) / (oh > 1 ? oh - 1 : 1);
        const int y0 = fy >> 8, wy = fy & 0xFF;
        const int y1 = (y0 + 1 < kAmbH) ? y0 + 1 : y0;
        uint8_t* p = out + static_cast<size_t>(y) * ow * 3;
        for (int x = 0; x < ow; ++x, p += 3) {
            const int fx = (x * (kAmbW - 1) * 256) / (ow > 1 ? ow - 1 : 1);
            const int x0 = fx >> 8, wx = fx & 0xFF;
            const int x1 = (x0 + 1 < kAmbW) ? x0 + 1 : x0;
            for (int c = 0; c < 3; ++c) {
                const int a = soft[y0][x0][c] + (((soft[y0][x1][c] - soft[y0][x0][c]) * wx) >> 8);
                const int b = soft[y1][x0][c] + (((soft[y1][x1][c] - soft[y1][x0][c]) * wx) >> 8);
                const int v = a + (((b - a) * wy) >> 8);
                p[c] = static_cast<uint8_t>((v * 38) / 100);   // deutlich abgedunkelt
            }
        }
    }
}

// ── Spielbild einsetzen ───────────────────────────────────────────────
// (rx, ry) im gedrehten Bild -> (gx, gy) im PPU-Rahmen.
inline void unrotate(int rot, int rx, int ry, int gw, int gh, int* gx, int* gy) {
    switch (rot & 3) {
        case 1: *gx = ry;          *gy = gh - 1 - rx; break;   //  90 Grad
        case 2: *gx = gw - 1 - rx; *gy = gh - 1 - ry; break;   // 180 Grad
        case 3: *gx = gw - 1 - ry; *gy = rx;          break;   // 270 Grad
        default: *gx = rx;         *gy = ry;          break;
    }
}

// Umkehrung von unrotate: Spielpunkt -> Punkt im gedrehten Bild.
inline void rotate_fwd(int rot, int gx, int gy, int gw, int gh, int* rx, int* ry) {
    switch (rot & 3) {
        case 1: *rx = gh - 1 - gy; *ry = gx;          break;
        case 2: *rx = gw - 1 - gx; *ry = gh - 1 - gy; break;
        case 3: *rx = gy;          *ry = gw - 1 - gx; break;
        default: *rx = gx;         *ry = gy;          break;
    }
}

void blit_game(const uint8_t* game, int gw, int gh, uint8_t* out, int ow, int oh,
               int dx, int dy, int dw, int dh, int rot) {
    const bool swap = (rot & 1) != 0;
    const int rw = swap ? gh : gw;
    const int rh = swap ? gw : gh;
    if (dw <= 0 || dh <= 0) return;
    for (int y = 0; y < dh; ++y) {
        const int oy = dy + y;
        if (oy < 0 || oy >= oh) continue;
        const int ry = (y * rh) / dh;
        uint8_t* p = out + (static_cast<size_t>(oy) * ow + (dx > 0 ? dx : 0)) * 3;
        for (int x = (dx < 0 ? -dx : 0); x < dw; ++x) {
            const int ox = dx + x;
            if (ox >= ow) break;
            const int rx = (x * rw) / dw;
            int gx = 0, gy = 0;
            unrotate(rot, rx, ry, gw, gh, &gx, &gy);
            const uint8_t* src = game + (static_cast<size_t>(gy) * gw + gx) * 3;
            uint8_t* dst = p + static_cast<size_t>(ox - (dx > 0 ? dx : 0)) * 3;
            dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];
        }
    }
}

// ── Menue ─────────────────────────────────────────────────────────────
const char* random_label(int m) {
    switch (m) {
        case kRandomGames: return "Microgames";
        case kRandomAll:   return "With bosses";
        default:           return "Off";
    }
}
const char* screen_label(int m) {
    switch (m) {
        case kScreenFit:     return "Fit";
        case kScreenStretch: return "Stretch";
        default:             return "Sharp";
    }
}

void item_text(int id, char* label, size_t ln, char* note, size_t nn,
               char* value, size_t vn) {
    note[0] = '\0';
    switch (id) {
        case kItemRandom:
            std::snprintf(label, ln, "Randomizer");
            std::snprintf(value, vn, "%s", random_label(g.random_mode)); break;
        case kItemSeed:
            std::snprintf(label, ln, "Seed");
            std::snprintf(value, vn, "%d", g.seed); break;
        case kItemKaizo:
            std::snprintf(label, ln, "KaizoWare");
            std::snprintf(note, nn, "(max level, fast)");
            std::snprintf(value, vn, "%s", g.kaizo ? "On" : "Off");
            break;
        case kItemBossRush:
            std::snprintf(label, ln, "Boss rush");
            std::snprintf(value, vn, "%s", g.boss_rush ? "On" : "Off");
            break;
        case kItemLives:
            std::snprintf(label, ln, "Infinite lives");
            std::snprintf(value, vn, "%s", g.inf_lives ? "On" : "Off"); break;
        case kItemSkip:
            std::snprintf(label, ln, "Cutscene skip");
            std::snprintf(value, vn, "%s", g.skip_scenes ? "Always" : "Normal");
            break;
        case kItemSpeed:
            std::snprintf(label, ln, "Game speed");
            std::snprintf(value, vn, "%d%%", g.speed_pct); break;
        case kItemReadout:
            std::snprintf(label, ln, "Microgame readout");
            std::snprintf(value, vn, "%s", g.readout ? "On" : "Off"); break;
        case kItemTouch:
            std::snprintf(label, ln, "Touch input");
            std::snprintf(note, nn, "(mouse / finger)");
            std::snprintf(value, vn, "%s",
                          g.touch_mode == kTouchModeDrag  ? "Drag" :
                          g.touch_mode == kTouchModeFlick ? "Flick" : "Off");
            break;
        case kItemMusicVol:
            std::snprintf(label, ln, "Music volume");
            std::snprintf(value, vn, "%d%%", g.music_vol); break;
        case kItemSfxVol:
            std::snprintf(label, ln, "Sound volume");
            std::snprintf(value, vn, "%d%%", g.sfx_vol); break;
        case kItemMenuMusic:
            std::snprintf(label, ln, "Menu music");
            std::snprintf(value, vn, "%s", g.menu_music ? "Title theme" : "Off"); break;
        case kItemWide:
            std::snprintf(label, ln, "Widescreen");
            std::snprintf(note, nn, "(May have bugs)");
            std::snprintf(value, vn, "%s", kWideLabels[g.wide]);
            break;
        case kItemScreen:
            std::snprintf(label, ln, "Scaling");
            std::snprintf(value, vn, "%s", screen_label(g.screen)); break;
        case kItemRotation:
            std::snprintf(label, ln, "Rotation");
            std::snprintf(value, vn, "%d deg", g.rotation * 90); break;
        case kItemBackdrop:
            std::snprintf(label, ln, "Backdrop");
            std::snprintf(value, vn, "%s",
                          g.backdrop == kBackdropBlack ? "Black" : "Ambient"); break;
        default: label[0] = value[0] = '\0'; break;
    }
}

// Breite des Menues in Zeichen, und ein Wertfeld fester Breite, damit die
// Pfeile in allen Zeilen uebereinanderstehen. Das Feld fasst den breitesten
// Wert ("Title theme", "With bosses"); mit den Pfeilen belegt es die letzten 15
// Spalten. Die laengste Beschriftung, "Touch input (mouse / finger)", endet
// in Spalte 29 -- bei 46 Spalten bleibt davor genau eine Spalte Luft.
constexpr int kMenuCols  = 46;
constexpr int kValueCols = 11;

// Alle Abstaende in Einheiten von sc, damit Hoehe, Zeichnen und Trefferflaechen
// aus derselben Rechnung stammen.
constexpr int kPadTop = 4, kPadBot = 4, kGapRule = 4, kGapFoot = 3;
constexpr int kFootH = WW_FONT_H + 6;   // Fusszeile mit Schliessen-Knopf
// Drei Trennlinien (kRuleAfterA/B/C). Die Hoehenrechnung zaehlte hier lange
// nur zwei -- seit Boss Rush sass die Fusszeile deshalb auf dem Rahmen.
constexpr int kMenuRules = 3;
inline int menu_line_units()  { return WW_FONT_H + 3; }
inline int menu_width_units() { return kMenuCols * (WW_FONT_W + 1) + 12; }
inline int menu_height_units() {
    return kPadTop + menu_line_units() + kGapRule
         + kItemCount * menu_line_units() + kMenuRules * kGapRule
         + kGapFoot + kFootH + kPadBot;
}

// Groesster Massstab, bei dem das Menue noch ganz in die Flaeche passt.
int menu_scale(int w, int h) {
    for (int sc = 6; sc > 1; --sc)
        if (menu_width_units() * sc <= w - 4 && menu_height_units() * sc <= h - 4)
            return sc;
    return 1;
}

void draw_frame(uint8_t* fb, int w, int h, int x, int y, int bw, int bh, int t, Rgb c) {
    fill_rect(fb, w, h, x, y, bw, t, c, 100);
    fill_rect(fb, w, h, x, y + bh - t, bw, t, c, 100);
    fill_rect(fb, w, h, x, y, t, bh, c, 100);
    fill_rect(fb, w, h, x + bw - t, y, t, bh, c, 100);
}

void draw_menu(uint8_t* fb, int w, int h, int sc) {
    MenuLayout& L = g.ml;
    const int cw   = (WW_FONT_W + 1) * sc;
    const int base = menu_line_units() * sc;
    const int padx = 6 * sc;
    const int pw = menu_width_units() * sc;
    // Die Breite bestimmt die Schriftgroesse, die Hoehe nicht. Was an Hoehe
    // uebrig ist -- im Hochformat reichlich --, geht in die Zeilen, bis sie
    // knapp doppelt so hoch sind: groessere Ziele fuer den Finger, gleiche
    // Schrift.
    const int spare = (h - 8 * sc) - menu_height_units() * sc;
    const int rpad  = (spare > 0) ? std::min(spare / kItemCount, 8 * sc) : 0;
    const int line  = base + rpad;
    const int ph = menu_height_units() * sc + kItemCount * rpad;
    const int px = (w - pw) / 2;
    const int py = (h - ph) / 2;
    L.valid = true;
    L.sc = sc; L.px = px; L.py = py; L.pw = pw; L.ph = ph; L.row_h = line;

    fill_rect(fb, w, h, px, py, pw, ph, Rgb{10, 12, 28}, 93);
    draw_frame(fb, w, h, px, py, pw, ph, sc, kColRule);

    int y = py + kPadTop * sc;
    draw_text(fb, w, h, px + padx, y, "WARIOWARE MOD MENU", kColTitle, sc);
    y += base;
    fill_rect(fb, w, h, px + padx, y, pw - 2 * padx, sc, kColRule, 100);
    y += kGapRule * sc;
    int top = y - sc;

    const bool pressing = g.ui.down && g.ui.press_row >= 0;
    char label[48], note[32], value[32];
    for (int i = 0; i < kItemCount; ++i) {
        const bool sel = (i == g.cursor);
        item_text(i, label, sizeof(label), note, sizeof(note),
                  value, sizeof(value));
        L.row_top[i] = top;           // die Zeile als Trefferflaeche: top .. top+line
        y = top + (line - WW_FONT_H * sc) / 2;   // Schrift mittig in der Zeile
        if (sel) {
            fill_rect(fb, w, h, px + 2 * sc, top, pw - 4 * sc, line, Rgb{60, 70, 130}, 78);
            fill_rect(fb, w, h, px + 2 * sc, top, 2 * sc, line, kColPick, 100);
        }

        // Wertfeld zwischen zwei Pfeilen. Die Pfeile sind echte Knoepfe: ein
        // Tipp aendert, Halten wiederholt. Das ganze Feld dazwischen schaltet
        // weiter -- ein grosses Ziel fuer den Finger.
        const int arr_r = px + pw - padx - cw;              // '>'
        const int f1    = arr_r - cw;                       // Feldende
        const int f0    = f1 - kValueCols * cw;             // Feldanfang
        const int arr_l = f0 - 2 * cw;                      // '<'
        const int vx0   = f0 + (kValueCols * cw - text_width(value, sc)) / 2;
        L.val_x0[i] = f0;
        L.val_x1[i] = f1;
        L.arl_x0[i] = arr_l - cw;     // eine Spalte Luft fuer den Finger

        const bool hot_l = pressing && g.ui.press_row == i && g.ui.press_target == kUiArrowL;
        const bool hot_r = pressing && g.ui.press_row == i && g.ui.press_target == kUiArrowR;
        const bool hot_v = pressing && g.ui.press_row == i && g.ui.press_target == kUiValue;
        if (hot_l) fill_rect(fb, w, h, arr_l - sc, top, cw + 2 * sc, line, kColPick, 40);
        if (hot_r) fill_rect(fb, w, h, arr_r - sc, top, cw + 2 * sc, line, kColPick, 40);
        if (hot_v) fill_rect(fb, w, h, f0 - sc, top, f1 - f0 + 2 * sc, line, kColPick, 25);
        const Rgb arrow = sel ? kColPick : kColRule;
        draw_text(fb, w, h, arr_l, y, "<", hot_l ? kColText : arrow, sc);
        draw_text(fb, w, h, arr_r, y, ">", hot_r ? kColText : arrow, sc);
        draw_text(fb, w, h, vx0, y, value, sel ? kColPick : kColValue, sc);

        const int lx = px + padx + 2 * cw;
        draw_text(fb, w, h, lx, y, label, sel ? kColText : kColDim, sc);
        if (note[0]) {
            // Der Hinweis steht immer gedaempft daneben, auch in der
            // ausgewaehlten Zeile -- er gehoert zur Sache, nicht zur Auswahl.
            draw_text(fb, w, h, lx + text_width(label, sc) + cw, y, note, kColRule, sc);
        }
        top += line;
        if (i == kRuleAfterA || i == kRuleAfterB || i == kRuleAfterC) {
            fill_rect(fb, w, h, px + padx, top + sc, pw - 2 * padx, sc, kColRule, 60);
            top += kGapRule * sc;
        }
    }

    // Fusszeile: Hinweis links, Schliessen-Knopf rechts.
    y = top + sc + kGapFoot * sc;
    const int foot_h = kFootH * sc;
    const char* btn = "Close  F10";
    const int bw = text_width(btn, sc) + 2 * cw;
    const int bx = px + pw - padx - bw;
    L.close_x0 = bx;      L.close_y0 = y;
    L.close_x1 = bx + bw; L.close_y1 = y + foot_h;
    const bool hot_c = g.ui.down && g.ui.press_target == kUiClose;
    fill_rect(fb, w, h, bx, y, bw, foot_h, hot_c ? kColPick : Rgb{40, 46, 90},
              hot_c ? 55 : 85);
    draw_frame(fb, w, h, bx, y, bw, foot_h, sc, hot_c ? kColPick : kColRule);
    const int ty = y + (foot_h - WW_FONT_H * sc) / 2;
    draw_text(fb, w, h, bx + cw, ty, btn, hot_c ? kColText : kColValue, sc);
    draw_text(fb, w, h, px + padx, ty, "Tap or D-pad   L/R coarse", kColDim, sc);
}

// Menueknopf oben rechts: fuer Maus und Finger der Weg ins Menue ohne F10.
void draw_menu_button(uint8_t* fb, int w, int h, int sc) {
    ButtonRect& b = g.mbtn;
    b.visible = false;
    if (g.open || g.ui.activity <= 0) return;
    const int size = 16 * sc, margin = 6 * sc;
    b.x0 = w - margin - size; b.y0 = margin;
    b.x1 = b.x0 + size;       b.y1 = b.y0 + size;
    b.visible = true;
    // Die letzte halbe Sekunde blendet er aus, statt hart zu verschwinden.
    const int fade = (g.ui.activity >= 30) ? 100 : g.ui.activity * 100 / 30;
    const bool hot = g.ui.down && g.ui.press_target == kUiMenuButton;
    fill_rect(fb, w, h, b.x0, b.y0, size, size, Rgb{10, 12, 28}, (hot ? 90 : 70) * fade / 100);
    const Rgb edge = hot ? kColPick : kColRule;
    fill_rect(fb, w, h, b.x0, b.y0, size, sc, edge, fade);
    fill_rect(fb, w, h, b.x0, b.y1 - sc, size, sc, edge, fade);
    fill_rect(fb, w, h, b.x0, b.y0, sc, size, edge, fade);
    fill_rect(fb, w, h, b.x1 - sc, b.y0, sc, size, edge, fade);
    const Rgb bar = hot ? kColPick : kColText;
    for (int k = 0; k < 3; ++k)
        fill_rect(fb, w, h, b.x0 + 4 * sc, b.y0 + (4 + 3 * k) * sc, size - 8 * sc, 2 * sc,
                  bar, fade);
}

// Ohne sichtbare Rueckmeldung fuehlt sich erfundene Eingabe kaputt an.
// Ring am Zeiger, gefuellt solange gedrueckt, plus die Ziehrichtung als
// Linie vom Aufsetzpunkt.
void draw_pointer(uint8_t* fb, int w, int h, int sc) {
    if (!g.ptr_valid || g.geo_dw <= 0) return;
    auto to_surface = [&](int gx, int gy, int* sx, int* sy) {
        const bool swap = (g.geo_rot & 1) != 0;
        const int rw = swap ? g.geo_gh : g.geo_gw;
        const int rh = swap ? g.geo_gw : g.geo_gh;
        int rx = 0, ry = 0;
        rotate_fwd(g.geo_rot, gx, gy, g.geo_gw, g.geo_gh, &rx, &ry);
        *sx = g.geo_dx + rx * g.geo_dw / rw;
        *sy = g.geo_dy + ry * g.geo_dh / rh;
    };
    int cx = 0, cy = 0;
    to_surface(g.ptr_gx, g.ptr_gy, &cx, &cy);
    const Rgb col = g.pointer_down ? kColPick : kColValue;
    const int t = (sc < 2) ? 2 : sc;          // Strichstaerke
    const int r = 8 * sc;                     // Armlaenge des Fadenkreuzes
    const int gap = 3 * sc;                   // Loch in der Mitte

    // Jede Form zweimal: erst schwarz und eine Spur groesser, dann in Farbe.
    // Sonst verschwindet der Zeiger auf hellem Spielbild.
    auto bar = [&](int x, int y, int bw, int bh) {
        fill_rect(fb, w, h, x - t / 2, y - t / 2, bw + t, bh + t, Rgb{0, 0, 0}, 85);
        fill_rect(fb, w, h, x, y, bw, bh, col, 100);
    };

    if (g.pointer_down) {
        int ox = 0, oy = 0;
        to_surface(g.ptr_ox, g.ptr_oy, &ox, &oy);
        const int steps = 40;
        for (int i = 0; i <= steps; ++i) {
            const int x = ox + (cx - ox) * i / steps;
            const int y = oy + (cy - oy) * i / steps;
            fill_rect(fb, w, h, x - t, y - t, 2 * t, 2 * t, Rgb{0, 0, 0}, 70);
        }
        for (int i = 0; i <= steps; ++i) {
            const int x = ox + (cx - ox) * i / steps;
            const int y = oy + (cy - oy) * i / steps;
            fill_rect(fb, w, h, x - t / 2, y - t / 2, t, t, kColPick, 100);
        }
        // Aufsetzpunkt markieren
        bar(ox - 2 * sc, oy - t / 2, 4 * sc, t);
        bar(ox - t / 2, oy - 2 * sc, t, 4 * sc);
    }

    // Fadenkreuz mit Loch in der Mitte, damit es das Ziel nicht verdeckt.
    bar(cx - r, cy - t / 2, r - gap, t);
    bar(cx + gap, cy - t / 2, r - gap, t);
    bar(cx - t / 2, cy - r, t, r - gap);
    bar(cx - t / 2, cy + gap, t, r - gap);
    if (g.pointer_down) bar(cx - 2 * sc, cy - 2 * sc, 4 * sc, 4 * sc);
}

void draw_readout(uint8_t* fb, int w, int h, int sc) {
    if (!in_gameplay()) return;
    char line[80];
    const int id = bus_read_u8(gp(kOffMicrogameId));
    if (g.random_mode != kRandomOff && g.last_original >= 0) {
        std::snprintf(line, sizeof(line), "MG %03d <- %03d  stage %d  lvl %d",
                      id, g.last_original, static_cast<int>(bus_read_u8(kStageId)),
                      static_cast<int>(bus_read_u8(gp(kOffDifficulty))));
    } else {
        std::snprintf(line, sizeof(line), "MG %03d  stage %d  lvl %d", id,
                      static_cast<int>(bus_read_u8(kStageId)),
                      static_cast<int>(bus_read_u8(gp(kOffDifficulty))));
    }
    // In ein schmales Hochkantfenster passt die Zeile im vollen Massstab
    // nicht mehr; lieber kleiner als abgeschnitten.
    while (sc > 1 && text_width(line, sc) > w - 6 * sc) --sc;
    draw_text_shadow(fb, w, h, 3 * sc, h - (WW_FONT_H + 4) * sc, line, kColValue, sc);
}

// ── Eingabe ───────────────────────────────────────────────────────────
// GBA-KEYINPUT ist aktiv-niedrig: 1 = losgelassen.
enum {
    kKeyA = 1 << 0, kKeyB = 1 << 1, kKeySel = 1 << 2, kKeySta = 1 << 3,
    kKeyRight = 1 << 4, kKeyLeft = 1 << 5, kKeyUp = 1 << 6, kKeyDown = 1 << 7,
    kKeyR = 1 << 8, kKeyL = 1 << 9
};
inline bool down(uint16_t k, int bit) { return (k & bit) == 0; }

int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

void adjust(int id, int delta, bool coarse) {
    const int step = coarse ? 10 : 1;
    const int dir  = (delta < 0) ? -1 : 1;
    switch (id) {
        case kItemRandom:
            g.random_mode += dir;
            if (g.random_mode < 0) g.random_mode = kRandomAll;
            if (g.random_mode > kRandomAll) g.random_mode = kRandomOff;
            break;
        case kItemSeed:
            g.seed += dir * (coarse ? 100 : 1);
            if (g.seed < 1) g.seed = 9999;
            if (g.seed > 9999) g.seed = 1;
            break;
        case kItemKaizo:    g.kaizo = !g.kaizo; break;
        case kItemBossRush: g.boss_rush = !g.boss_rush; break;
        case kItemLives:    g.inf_lives = !g.inf_lives; break;
        case kItemSkip:     g.skip_scenes = !g.skip_scenes; break;
        case kItemSpeed:    g.speed_pct = clampi(g.speed_pct + dir * 10, 50, 300); break;
        case kItemReadout:  g.readout = !g.readout; break;
        case kItemTouch:
            g.touch_mode += dir;
            if (g.touch_mode < 0) g.touch_mode = kTouchModeFlick;
            if (g.touch_mode > kTouchModeFlick) g.touch_mode = kTouchModeOff;
            break;
        case kItemMusicVol: g.music_vol = clampi(g.music_vol + dir * step * 5, 0, 100); break;
        case kItemSfxVol:   g.sfx_vol   = clampi(g.sfx_vol   + dir * step * 5, 0, 100); break;
        case kItemMenuMusic: g.menu_music = !g.menu_music; break;
        case kItemWide:
            g.wide = (g.wide + dir + kWideCount) % kWideCount;
            break;
        case kItemScreen:
            g.screen += dir;
            if (g.screen < 0) g.screen = kScreenStretch;
            if (g.screen > kScreenStretch) g.screen = kScreenSharp;
            break;
        case kItemRotation: g.rotation = (g.rotation + dir + 4) & 3; break;
        case kItemBackdrop: g.backdrop = (g.backdrop == kBackdropBlack)
                                             ? kBackdropAmbient : kBackdropBlack; break;
        default: break;
    }
}

// Breite der PPU-Ansicht. Bei kWideAuto folgt sie dem Fensterformat: die
// Hoehe bleibt 160 Zeilen, also ist die passende Breite
// Fensterbreite * 160 / Fensterhoehe. Gerade gerundet, damit die beiden
// Raender gleich gross werden, und auf 240..480 begrenzt.
int wide_width() {
    if (g.wide != kWideAuto) return kWideSteps[g.wide];
    if (g.win_w < 1 || g.win_h < 1) return 240;
    long w = (static_cast<long>(g.win_w) * 160 + g.win_h / 2) / g.win_h;
    w &= ~1L;
    if (w < 240) w = 240;
    if (w > 480) w = 480;
    return static_cast<int>(w);
}

// ── Zeiger: einlesen, umrechnen, in Tasten uebersetzen ────────────────
//
// Der Zeiger kommt als *Zustand*, nicht als Ereignis: win.pump() hat die
// SDL-Ereigniswarteschlange schon geleert, bevor der Mod an der Reihe ist.
// SDL_GetMouseState und SDL_GetTouchFinger liefern genau das.
bool poll_pointer(int* wx, int* wy, bool* downp) {
#ifdef GBARECOMP_HAVE_SDL2
    // Finger haben Vorrang vor der Maus.
    const int devs = SDL_GetNumTouchDevices();
    for (int i = 0; i < devs; ++i) {
        const SDL_TouchID id = SDL_GetTouchDevice(i);
        if (SDL_GetNumTouchFingers(id) <= 0) continue;
        if (const SDL_Finger* f = SDL_GetTouchFinger(id, 0)) {
            *wx = static_cast<int>(f->x * static_cast<float>(g.win_w));
            *wy = static_cast<int>(f->y * static_cast<float>(g.win_h));
            *downp = true;
            return true;
        }
    }
    int mx = 0, my = 0;
    const Uint32 b = SDL_GetMouseState(&mx, &my);
    *wx = mx; *wy = my;
    *downp = (b & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    return true;
#else
    (void)wx; (void)wy; (void)downp;
    return false;
#endif
}

// Fensterpunkt -> Spielpunkt. Zwei Stufen rueckwaerts: erst die Abbildung,
// mit der das Fenster die Zeichenflaeche zeigt (dieselbe Funktion, die
// HostWindow::present benutzt), dann die Abbildung, mit der compose das
// Spielbild in die Flaeche gesetzt hat -- Massstab, Zentrierung, Drehung.
// Erste Stufe allein: Fensterpunkt -> Zeichenflaeche. Das Menue lebt in der
// Flaeche, das Spiel eine Stufe tiefer.
bool window_to_surface(int wx, int wy, int* sx, int* sy) {
    if (g.geo_sw <= 0 || g.geo_sh <= 0 || g.win_w <= 0 || g.win_h <= 0) return false;
    const gbarecomp::PresentationLayout lay =
        gbarecomp::compute_presentation_layout(g.win_w, g.win_h, g.geo_sw, g.geo_sh);
    if (lay.width <= 0 || lay.height <= 0) return false;
    *sx = static_cast<int>(static_cast<long long>(wx - lay.x) * g.geo_sw / lay.width);
    *sy = static_cast<int>(static_cast<long long>(wy - lay.y) * g.geo_sh / lay.height);
    return *sx >= 0 && *sy >= 0 && *sx < g.geo_sw && *sy < g.geo_sh;
}

bool window_to_game(int wx, int wy, int* gx, int* gy) {
    if (g.geo_dw <= 0 || g.geo_dh <= 0) return false;
    int sx = 0, sy = 0;
    if (!window_to_surface(wx, wy, &sx, &sy)) return false;

    const bool swap = (g.geo_rot & 1) != 0;
    const int rw = swap ? g.geo_gh : g.geo_gw;
    const int rh = swap ? g.geo_gw : g.geo_gh;
    const int rx = static_cast<int>(
        static_cast<long long>(sx - g.geo_dx) * rw / g.geo_dw);
    const int ry = static_cast<int>(
        static_cast<long long>(sy - g.geo_dy) * rh / g.geo_dh);
    if (rx < 0 || ry < 0 || rx >= rw || ry >= rh) return false;
    unrotate(g.geo_rot, rx, ry, g.geo_gw, g.geo_gh, gx, gy);
    return true;
}

// Welches Schema fuer das laufende Microgame gilt.
int touch_scheme() {
    if (g.touch_mode == kTouchModeOff) return kTouchOff;
    const int fallback = (g.touch_mode == kTouchModeFlick) ? kTouchFlick : kTouchBoth;
    if (!in_gameplay()) return fallback;
    const int id = bus_read_u8(gp(kOffMicrogameId));
    if (id < 0 || id >= kMicrogameCount) return fallback;
    const int t = g.touch_table[id];
    return (t == kTouchInherit) ? fallback : t;
}

// Erfundene Tastendruecke aus der Zeigerbewegung. Rueckgabe: aktiv-niedrige
// Maske der zusaetzlich gedrueckten Tasten (0 = nichts).
// Der Kern, getrennt vom Einlesen -- so laesst er sich ohne SDL und ohne
// Hand am Zeiger pruefen (ww_mod_preview --selftest).
uint16_t pointer_keys_from(int wx, int wy, bool down, int scheme) {
    int gx = 0, gy = 0;
    const bool inside = window_to_game(wx, wy, &gx, &gy);
    g.ptr_valid = inside;
    if (inside) { g.ptr_gx = gx; g.ptr_gy = gy; }
    g.pointer_down = down && inside;

    uint16_t bits = 0;

    // Aufsetzen
    if (g.pointer_down && !g.pointer_was) {
        g.ptr_ox = gx; g.ptr_oy = gy;
        g.flick_fired = false;
        if (scheme == kTouchTap || scheme == kTouchBoth)
            g.touch_press_left = kTouchPressPumps;
    }
    if (!g.pointer_down) g.flick_fired = false;
    g.pointer_was = g.pointer_down;

    // Laufender A-Impuls, oder A durchgehend gehalten.
    if (g.touch_press_left > 0) { --g.touch_press_left; bits |= kKeyA; }
    if (scheme == kTouchHold && g.pointer_down) bits |= kKeyA;

    // Richtung
    const int dx = gx - g.ptr_ox, dy = gy - g.ptr_oy;
    auto dominant = [&](int dist) -> uint16_t {
        if (std::abs(dx) < dist && std::abs(dy) < dist) return 0;
        if (std::abs(dx) >= std::abs(dy)) return (dx > 0) ? kKeyRight : kKeyLeft;
        return (dy > 0) ? kKeyDown : kKeyUp;
    };

    if (g.pointer_down && (scheme == kTouchDrag || scheme == kTouchBoth)) {
        bits |= dominant(kTouchDeadZone);
    } else if (scheme == kTouchFlick) {
        if (g.pointer_down && !g.flick_fired) {
            const uint16_t d = dominant(kTouchFlickDist);
            if (d) {
                g.flick_fired = true;
                g.touch_dir_bits = d;
                g.touch_dir_left = kTouchPressPumps;
                // Der Aufsetzpunkt wandert mit, damit ein zweites Wischen in
                // dieselbe Richtung wieder ausloest.
                g.ptr_ox = gx; g.ptr_oy = gy;
            }
        }
    }
    if (g.touch_dir_left > 0) { --g.touch_dir_left; bits |= g.touch_dir_bits; }

    g.touch_out = bits;
    return bits;
}

// Schema-Tabelle: eine Zeile je Microgame, "<Nummer> = <schema>". Fehlt die
// Datei, bekommt jedes Spiel die Voreinstellung aus dem Menue. Die Tabelle
// ist der Kern der Sache -- ob ein Microgame Pegel oder Flanke liest, steht
// in seinem Code und laesst sich nur durch Ausprobieren eintragen.
void load_touch_table(const char* path) {
    const char* p = (path && path[0]) ? path : "touch.ini";
    std::FILE* f = std::fopen(p, "r");
    if (!f) return;
    char line[128];
    int n = 0;
    while (std::fgets(line, sizeof(line), f)) {
        char* c = line;
        while (*c == ' ' || *c == '\t') ++c;
        if (*c == '#' || *c == ';' || *c == '\n' || !*c) continue;
        int id = 0; char word[32] = {};
        if (std::sscanf(c, "%d = %31s", &id, word) != 2 &&
            std::sscanf(c, "%d=%31s", &id, word) != 2) continue;
        if (id < 0 || id >= kMicrogameCount) continue;
        int v = kTouchInherit;
        if      (!std::strcmp(word, "off"))   v = kTouchOff;
        else if (!std::strcmp(word, "tap"))   v = kTouchTap;
        else if (!std::strcmp(word, "hold"))  v = kTouchHold;
        else if (!std::strcmp(word, "drag"))  v = kTouchDrag;
        else if (!std::strcmp(word, "both"))  v = kTouchBoth;
        else if (!std::strcmp(word, "flick")) v = kTouchFlick;
        else continue;
        g.touch_table[id] = static_cast<unsigned char>(v);
        ++n;
    }
    std::fclose(f);
    std::fprintf(stderr, "[ww-mod] Zeigertabelle \"%s\": %d Eintraege\n", p, n);
}

bool f10_edge() {
#ifdef GBARECOMP_HAVE_SDL2
    const Uint8* ks = SDL_GetKeyboardState(nullptr);
    if (!ks) return false;
    const bool now = ks[SDL_SCANCODE_F10] != 0;
    const bool edge = now && !g.prev_f10;
    g.prev_f10 = now;
    return edge;
#else
    return false;
#endif
}

// Beim Oeffnen und Schliessen einen kurzen Tastenimpuls einschieben, mit dem
// das Spiel selbst in seine Pause geht und wieder heraus. Das ist besser als
// den Gast anzuhalten: seine Soundengine laeuft weiter, und die Pause bringt
// alle Nebenwirkungen mit, die das Spiel dafuer vorgesehen hat.
void queue_press(uint16_t bits) { g.inject_bits = bits; g.inject_left = 6; }

void menu_opened() {
    g.hold_frames = 0;
    g.held_dir = 0;
    // Ein laufender Weg durch das Spielmenue endet hier. Sonst liefe er
    // hinter dem Mod-Menue weiter und fuehrte den Klick von vorhin aus,
    // sobald es wieder zugeht -- egal ob F10, Tastenkombination oder Zeiger.
    g.plan.active = false;
    g.press_hit = GameHit{};
    g.game_ptr_down = false;
    if (in_gameplay() && gameplay_state() == kStateRunning) queue_press(kKeySta);
    if (g.menu_music) {
        g.saved_song = -1;
        const uint32_t sp = bus_read_u32(kSoundPlayerTable);   // Spieler 0 = Haupt-BGM
        if (is_ram(sp)) {
            const uint32_t song = bus_read_u32(sp + kOffPlayerSong);
            if (is_rom(song)) g.saved_song = static_cast<int>(bus_read_u32(song + kOffSongNum));
        }
        g.pending_song = kSongTitleTheme;
    }
}

void menu_closed() {
    g.frozen.clear();
    g.frozen.shrink_to_fit();
    g.frozen_w = g.frozen_h = 0;
    if (in_gameplay() && gameplay_state() == kStatePaused) queue_press(kKeyA);
    if (g.menu_music && g.saved_song >= 0) {
        g.pending_song = g.saved_song;
        g.saved_song = -1;
    }
}

// ── Die Menues des Spiels per Zeiger ──────────────────────────────────
//
// Das Spiel kennt keinen Zeiger. Sein Hauptmenue haengt an einer gezeichneten
// Hand, die mit dem Steuerkreuz von Ordner zu Ordner wandert; A oeffnet, B
// geht zurueck. Ein Klick kann deshalb nur eines tun: dieselben Tasten
// erfinden, die ein Spieler druecken wuerde. Nichts wird umgeschrieben, keine
// Menuefunktion von aussen gerufen -- die Wege, die Klaenge und die Sperren
// gegen verschlossene Ordner bleiben Sache des Spiels.
//
// Woher die Zahlen stammen:
//
//   D_03006518   +0 Ordner, +1 Zustand, +2 Reiter, +3/+4 Gitterzelle
//   D_083AA0C4   28 Ordner des Reiters "Games", je 16 Byte: +4..+7 die
//                Nachbarn oben/unten/links/rechts, +0xC zeigt auf die
//                Handposition (asm_08012828.s, asm_08012700.s)
//   D_083AA294   dieselben neun Ordner in der Anordnung des Reiters "Grid"
//   D_083A9CE0   drei Handpositionen, eine je Reiter
//   D_083AB2CC   drei Zeilen der Optionen (asm_080145d4.s)
//   D_083AB354   zwei Knoepfe der Sicherheitsabfrage (asm_08014a34.s)
//
// Zustaende (D_03006518+1, main_menu_scene_update): 0 Reiterleiste,
// 1 Ordner im Reiter "Games", 2 Ordner im Reiter "Grid", 3 Gitter der
// Microgames, 4 Optionen, 5 Sicherheitsabfrage, 6 Abspann-Ansicht.
//
// Die Handposition ist nicht die Mitte des Bildes darunter. Gemessen an einem
// gezeichneten Bild liegt der Ordner vier Punkte links und vier ueber der
// Hand und ist 25x20 Punkte gross; die Gitterzellen stehen in func_080136F4:
// 48 breit, 32 hoch, die Hand 42 rechts und 24 unter der linken oberen Ecke.
constexpr uint32_t kMenuVars   = 0x03006518u;  // D_03006518
constexpr uint32_t kGfxFlags   = 0x03004854u;  // gGraphicsBuffer.unk854, Bit 1 sperrt
constexpr uint32_t kBgOfs2X    = 0x03004014u;  // BG_OFS[2].x -- damit scrollt das Gitter
constexpr uint32_t kSavePtr    = 0x03003BBCu;  // Zeiger auf den Spielstand
constexpr uint32_t kTabPosTab  = 0x083A9CE0u;
constexpr uint32_t kGamesTab   = 0x083AA0C4u;
constexpr uint32_t kGridTab    = 0x083AA294u;
constexpr uint32_t kOptionTab  = 0x083AB2CCu;
constexpr uint32_t kDialogTab  = 0x083AB354u;
constexpr int kSceneMainMenu = 4;
constexpr int kGamesCount    = 28;
constexpr int kGridCount     = 9;
// MainMenuSceneData (src/scenes/main_menu.h), Puffer bei kSceneData
constexpr uint32_t kOffMenuOn   = 0x008u;   // unk8: Menue nimmt Eingaben an
constexpr uint32_t kOffBusy     = 0x0DDu;   // drei Bytes Beschaeftigt-Bits
constexpr uint32_t kOffGridCols = 0x0E8u;   // unkE8: Spalten des Gitters
constexpr uint32_t kOffGridMax  = 0x0F0u;   // unkF0: belegte Zellen
constexpr uint32_t kOffOptRow   = 0x0FDu;   // unkFD: Zeile der Optionen
constexpr uint32_t kOffDlgCol   = 0x14Cu;   // unk14C: Knopf der Abfrage

// Trefferflaechen der unteren Leiste, an einem gezeichneten Bild abgemessen.
constexpr int kTabX0[3] = { 18,  91, 163};
constexpr int kTabX1[3] = { 89, 161, 233};
constexpr int kBarY     = 143;          // darunter liegt die Leiste
constexpr int kBackX0   = 222;          // der Knopf "B <-" am rechten Rand

inline int menu_var(int i)  { return bus_read_u8(kMenuVars + static_cast<uint32_t>(i)); }
inline int menu_state()     { return menu_var(1); }
inline int menu_tab()       { return menu_var(2); }
inline int menu_entry()     { return menu_var(0); }
inline bool in_main_menu() {
    return static_cast<int16_t>(bus_read_u16(kCurrentScene)) == kSceneMainMenu;
}

// func_08011698: das Menue nimmt Eingaben nur an, wenn es eingeschaltet ist,
// die Grafik nicht blendet und keines der Beschaeftigt-Bits steht. Genau
// dieselbe Pruefung, damit ein erfundener Druck nie ins Leere faellt.
bool menu_ready() {
    if (!bus_read_u8(gp(kOffMenuOn))) return false;
    if (bus_read_u16(kGfxFlags) & 0x0002u) return false;
    const uint32_t busy = bus_read_u8(gp(kOffBusy)) |
                          bus_read_u8(gp(kOffBusy + 1)) |
                          (bus_read_u8(gp(kOffBusy + 2)) & 0x03u);
    return busy == 0;
}

// save_is_stage_unlocked: Bit 1 im Stage-Byte des Spielstands (memory.h,
// stageFlags[] ab 0x20, acht Byte je Eintrag).
bool stage_unlocked(int id) {
    if (id < 0 || id >= 32) return false;
    const uint32_t save = bus_read_u32(kSavePtr);
    if (!is_ram(save)) return false;
    return (bus_read_u8(save + 0x20u + static_cast<uint32_t>(id) * 8u) & 0x02u) != 0;
}

// Handposition eines Eintrags. Beide Tabellen sind gleich gebaut.
bool entry_pos(uint32_t table, int idx, int* x, int* y) {
    const uint32_t p = bus_read_u32(table + static_cast<uint32_t>(idx) * 16u + 12u);
    if (!is_rom(p)) return false;
    *x = static_cast<int16_t>(bus_read_u16(p));
    *y = static_cast<int16_t>(bus_read_u16(p + 2u));
    return true;
}

// Ein Schritt im Ordnernetz, so wie das Spiel ihn geht. Im Reiter "Games"
// (func_08012768 und die drei Geschwister) wird die Kette weiterverfolgt, bis
// ein erreichbarer Eintrag kommt; im Reiter "Grid" (func_08013460) zaehlt nur
// der direkte Nachbar. Erreichbar heisst: freigeschaltet oder unterhalb der
// festen Grenze, die das Spiel selbst setzt.
int folder_step(bool grid, int from, int dir) {
    const uint32_t table = grid ? kGridTab : kGamesTab;
    const int count = grid ? kGridCount : kGamesCount;
    const int limit = grid ? 8 : 10;
    auto link = [&](int i) {
        return static_cast<int>(static_cast<int8_t>(
            bus_read_u8(table + static_cast<uint32_t>(i) * 16u + 4u +
                        static_cast<uint32_t>(dir))));
    };
    int n = link(from);
    for (int guard = 0; guard < 32; ++guard) {
        if (n < 0 || n >= count) return -1;
        if (stage_unlocked(n) || n <= limit) return n;
        if (grid) return -1;
        n = link(n);
    }
    return -1;
}

// Kuerzester Weg durch dasselbe Netz: welche Richtung als erste zu druecken
// ist. Gesucht wird mit Breitensuche, damit auch der Umweg gefunden wird, den
// ein Spieler suchen muesste.
int folder_first_dir(bool grid, int from, int to) {
    const int count = grid ? kGridCount : kGamesCount;
    if (from == to || from < 0 || to < 0 || from >= count || to >= count) return -1;
    int first[kGamesCount];
    bool seen[kGamesCount] = {};
    int queue[kGamesCount], head = 0, tail = 0;
    seen[from] = true;
    queue[tail++] = from;
    first[from] = -1;
    while (head < tail) {
        const int cur = queue[head++];
        for (int d = 0; d < 4; ++d) {
            const int nxt = folder_step(grid, cur, d);
            if (nxt < 0 || nxt >= count || seen[nxt]) continue;
            seen[nxt] = true;
            first[nxt] = (cur == from) ? d : first[cur];
            if (nxt == to) return first[nxt];
            queue[tail++] = nxt;
        }
    }
    return -1;
}

// Dasselbe fuer das Gitter. Die Regeln stehen in func_080137B0: die Zeile
// bleibt zwischen 0 und 3, die Spalte zwischen 0 und unkE8-1, die letzte
// Spalte zwingt auf Zeile 3, und ein Feld jenseits von unkF0 gibt es nicht.
struct GridRules { int cols = 1; int cells = 1; };
GridRules grid_rules() {
    GridRules r;
    r.cols  = bus_read_u8(gp(kOffGridCols));
    r.cells = bus_read_u8(gp(kOffGridMax));
    if (r.cols < 1) r.cols = 1;
    if (r.cells < 1) r.cells = 1;
    return r;
}
bool grid_move(const GridRules& r, int x, int y, int dir, int* nx, int* ny) {
    int ax = x, ay = y;
    if (dir == 0 && ay != 0) --ay;
    if (dir == 1 && ay < 3)  ++ay;
    if (dir == 2 && ax != 0) --ax;
    if (dir == 3 && ax < r.cols - 1) ++ax;
    if (ax == r.cols - 1) ay = 3;
    if (ax == x && ay == y) return false;
    if (ax * 4 + ay >= r.cells) return false;
    *nx = ax; *ny = ay;
    return true;
}
int grid_first_dir(int fx, int fy, int tx, int ty) {
    const GridRules r = grid_rules();
    if (fx == tx && fy == ty) return -1;
    const int n = r.cols * 4;
    if (n <= 0 || n > 256) return -1;
    int first[256];
    bool seen[256] = {};
    int queue[256], head = 0, tail = 0;
    const int start = fx * 4 + fy;
    if (start < 0 || start >= n) return -1;
    seen[start] = true; queue[tail++] = start; first[start] = -1;
    while (head < tail) {
        const int cur = queue[head++];
        const int cx = cur / 4, cy = cur % 4;
        for (int d = 0; d < 4; ++d) {
            int ax = 0, ay = 0;
            if (!grid_move(r, cx, cy, d, &ax, &ay)) continue;
            const int idx = ax * 4 + ay;
            if (idx < 0 || idx >= n || seen[idx]) continue;
            seen[idx] = true;
            first[idx] = (cur == start) ? d : first[cur];
            if (ax == tx && ay == ty) return first[idx];
            queue[tail++] = idx;
        }
    }
    return -1;
}

// Spielpunkt -> was dort liegt. Der Zustand entscheidet mit: dieselbe Stelle
// am unteren Rand traegt im Zustand 0 die Reiter und sonst den Zurueck-Knopf.
GameHit game_menu_hit(int gx, int gy) {
    GameHit h;
    if (!in_main_menu()) return h;
    const int st  = menu_state();
    const int tab = menu_tab();

    if (gy >= kBarY) {
        if (st == 0) {
            for (int i = 0; i < 3; ++i)
                if (gx >= kTabX0[i] && gx < kTabX1[i]) { h.kind = kHitTab; h.a = i; return h; }
            return h;
        }
        if (gx >= kBackX0) { h.kind = kHitBack; return h; }
        return h;
    }

    if (st == 6) { h.kind = kHitViewer; return h; }   // Abspann: ein Tipp schliesst

    if (st == 5) {                                    // Sicherheitsabfrage
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        if (bus_read_u32(kDialogTab) == 0u) return h;
        x0 = static_cast<int16_t>(bus_read_u16(kDialogTab));
        y0 = static_cast<int16_t>(bus_read_u16(kDialogTab + 2u));
        x1 = static_cast<int16_t>(bus_read_u16(kDialogTab + 4u));
        y1 = static_cast<int16_t>(bus_read_u16(kDialogTab + 6u));
        const int ym = (y0 + y1) / 2;
        if (gy < ym - 14 || gy > ym + 10) return h;
        h.kind = kHitDialog;
        h.a = (gx < (x0 + x1) / 2) ? 0 : 1;
        return h;
    }

    // Optionen: drei Zeilen quer ueber das Bild, Hand am rechten Ende.
    if (st == 4 || (st == 0 && tab == 2)) {
        for (int i = 0; i < 3; ++i) {
            const int ry = static_cast<int16_t>(bus_read_u16(kOptionTab + i * 4u + 2u));
            if (gx >= 24 && gx < 200 && gy >= ry - 12 && gy < ry + 12) {
                h.kind = kHitOption; h.a = i; return h;
            }
        }
        return h;
    }

    // Gitter der Microgames. Die Zellen liegen im Hintergrund 2, der mitscrollt
    // -- deshalb entscheidet der laufende Versatz, welche Spalte gerade wo steht.
    if (st == 3 || (st == 2 && gx >= 86)) {
        const int scroll = static_cast<int16_t>(bus_read_u16(kBgOfs2X));
        if (gx >= 86 && gy >= 8 && gy < 136) {
            const int col = (gx + scroll + 2) / 48;
            int row = (gy - 8) / 32;
            if (col < 0 || row < 0 || row > 3 || col > 63) return h;
            if (st == 3) {
                // Im Gitter stehen Spaltenzahl und Feldzahl richtig.
                const GridRules r = grid_rules();
                if (col >= r.cols) return h;
                if (col == r.cols - 1) row = 3;   // die letzte Spalte hat nur eine Zeile
                if (col * 4 + row >= r.cells) return h;
            }
            // In der Vorschau (Zustand 2) gehoeren sie noch zur zuletzt
            // geoeffneten Stage; die Zelle wird erst im Gitter zurechtgerueckt.
            h.kind = kHitCell; h.a = col; h.b = row;
            return h;
        }
    }

    // Ordner. Welche Anordnung gilt, sagt der Reiter.
    if (st == 1 || st == 2 || st == 3 || st == 0) {
        const bool grid = (st == 2 || st == 3) || (st == 0 && tab == 1);
        const uint32_t table = grid ? kGridTab : kGamesTab;
        const int count = grid ? kGridCount : kGamesCount;
        const int limit = grid ? 8 : 10;
        for (int i = 0; i < count; ++i) {
            int px = 0, py = 0;
            if (!entry_pos(table, i, &px, &py)) continue;
            if (std::abs(gx - (px - 4)) > 13 || std::abs(gy - (py - 4)) > 10) continue;
            if (!stage_unlocked(i) && i > limit) return h;   // dort ist nichts
            h.kind = kHitFolder; h.a = i; h.b = grid ? 1 : 0;
            return h;
        }
    }
    return h;
}

// Wohin der Treffer gehoert: Reiter und Zustand, die erreicht sein muessen.
int hit_tab(const GameHit& h) {
    switch (h.kind) {
        case kHitTab:    return h.a;
        case kHitFolder: return h.b;          // 0 = Games, 1 = Grid
        case kHitCell:   return 1;
        case kHitOption: return 2;
        default:         return -1;
    }
}

// Ein Tastendruck je Aufruf, und nur wenn das Menue bereit ist. Was zu
// druecken ist, ergibt der Vergleich zwischen Ist und Soll; laeuft etwas
// schief, endet der Plan am Vorrat, ohne dass etwas haengen bleibt.
uint16_t plan_step() {
    GamePlan& p = g.plan;
    if (!p.active) return 0;
    if (!in_main_menu()) { p.active = false; return 0; }
    if (--p.budget <= 0) { p.active = false; return 0; }
    if (!menu_ready()) return 0;

    static const uint16_t kDirKey[4] = {kKeyUp, kKeyDown, kKeyLeft, kKeyRight};
    const GameHit w = p.want;
    const int st = menu_state();

    // WWMOD_MENU_TRACE=1 schreibt je Tastendruck eine Zeile: damit ist ohne
    // Zuschauen belegbar, dass ein Klick den Weg tatsaechlich geht.
    static const bool trace = std::getenv("WWMOD_MENU_TRACE") != nullptr;
    auto say = [&](uint16_t k) -> uint16_t {
        if (trace && k) {
            const char* n = (k == kKeyA) ? "A" : (k == kKeyB) ? "B"
                          : (k == kKeyUp) ? "hoch" : (k == kKeyDown) ? "runter"
                          : (k == kKeyLeft) ? "links" : "rechts";
            std::fprintf(stderr, "[ww-menu] Zustand %d Reiter %d Eintrag %d "
                         "Zelle %d/%d -> %s (Ziel %d/%d/%d, oeffnen %d)\n",
                         menu_state(), menu_tab(), menu_entry(),
                         menu_var(3), menu_var(4), n,
                         p.want.kind, p.want.a, p.want.b, p.open ? 1 : 0);
        }
        return k;
    };

    if (w.kind == kHitBack)   { p.active = false; return say(kKeyB); }
    if (w.kind == kHitViewer) { p.active = false; return say(kKeyA); }

    const int wt = hit_tab(w);

    switch (st) {
        case 0: {   // Reiterleiste
            const int tab = menu_tab();
            if (wt < 0) { p.active = false; return 0; }
            if (tab != wt) return say((tab < wt) ? kKeyRight : kKeyLeft);
            if (w.kind == kHitTab) { p.active = false; return p.open ? say(kKeyA) : 0; }
            return say(kKeyA);   // in den Reiter hinein
        }
        case 1:
        case 2: {   // Ordner, links die Anordnung des Reiters
            const bool grid = (st == 2);
            if (w.kind == kHitCell) {
                if (!grid) return say(kKeyB);
                return say(kKeyA);    // ins Gitter, den Rest macht Zustand 3
            }
            if (w.kind != kHitFolder || w.b != (grid ? 1 : 0)) return say(kKeyB);
            const int cur = menu_entry();
            if (cur != w.a) {
                const int d = folder_first_dir(grid, cur, w.a);
                if (d < 0) { p.active = false; return 0; }   // nicht erreichbar
                return say(kDirKey[d]);
            }
            p.active = false;
            return p.open ? say(kKeyA) : 0;
        }
        case 3: {   // Gitter der Microgames
            if (w.kind != kHitCell) return say(kKeyB);
            if (p.snap) {
                // Aus der Vorschau geklickt: jetzt gelten die richtigen
                // Grenzen. Die letzte Spalte kennt nur die unterste Zeile,
                // und hinter dem letzten Feld ist Schluss.
                const GridRules r = grid_rules();
                if (p.want.a > r.cols - 1) p.want.a = r.cols - 1;
                if (p.want.a == r.cols - 1) p.want.b = 3;
                while (p.want.a * 4 + p.want.b >= r.cells && p.want.b > 0) --p.want.b;
                if (p.want.a * 4 + p.want.b >= r.cells) { p.active = false; return 0; }
                p.snap = false;
            }
            const GameHit& w2 = p.want;
            const int cx = menu_var(3), cy = menu_var(4);
            if (cx != w2.a || cy != w2.b) {
                const int d = grid_first_dir(cx, cy, w2.a, w2.b);
                if (d < 0) { p.active = false; return 0; }
                return say(kDirKey[d]);
            }
            p.active = false;
            return p.open ? say(kKeyA) : 0;
        }
        case 4: {   // Optionen
            if (w.kind != kHitOption) return say(kKeyB);
            const int row = bus_read_u8(gp(kOffOptRow));
            if (row != w.a) return say((row < w.a) ? kKeyDown : kKeyUp);
            p.active = false;
            return p.open ? say(kKeyA) : 0;
        }
        case 5: {   // Sicherheitsabfrage
            if (w.kind != kHitDialog) { p.active = false; return 0; }
            const int col = bus_read_u8(gp(kOffDlgCol));
            if (col != w.a) return say((col < w.a) ? kKeyRight : kKeyLeft);
            p.active = false;
            return p.open ? say(kKeyA) : 0;
        }
        default:
            return 0;   // Uebergang: warten, der Vorrat deckt den Rest ab
    }
}

// Druck und Loslassen im Spielmenue. Wie im Mod-Menue entscheidet der Druck,
// was gemeint ist -- die Hand setzt sich sofort in Bewegung, das ist die
// Rueckmeldung --, und erst das Loslassen auf demselben Feld oeffnet. Wer
// danebengreift, zieht weg und es passiert nichts weiter.
void game_menu_pointer(int gx, int gy, bool valid, bool down) {
    const bool press   = down && !g.game_ptr_down && valid;
    const bool release = !down && g.game_ptr_down;
    g.game_ptr_down = down && valid;

    if (press) {
        const GameHit h = game_menu_hit(gx, gy);
        g.press_hit = h;
        g.plan.px = gx; g.plan.py = gy;
        if (h.kind != kHitNone && h.kind != kHitBack && h.kind != kHitViewer) {
            g.plan.active = true;
            g.plan.want   = h;
            g.plan.open   = false;
            g.plan.budget = 900;   // 15 Sekunden reichen fuer jeden Weg
            g.plan.snap   = (h.kind == kHitCell && menu_state() != 3);
        }
        return;
    }
    if (release && g.press_hit.kind != kHitNone) {
        // Zwischen Aufsetzen und Loslassen kann sich das Bild bewegt haben --
        // die Hand wandert, das Gitter scrollt. Deshalb zaehlt entweder
        // dasselbe Feld oder ein Finger, der liegen geblieben ist.
        const GameHit h = valid ? game_menu_hit(gx, gy) : GameHit{};
        const bool near = valid && std::abs(gx - g.plan.px) <= 6 &&
                                   std::abs(gy - g.plan.py) <= 6;
        if (same_hit(h, g.press_hit) || near) {
            const bool fresh = !g.plan.active || !same_hit(g.plan.want, g.press_hit);
            if (fresh) {
                g.plan.want = g.press_hit;
                g.plan.snap = (g.press_hit.kind == kHitCell && menu_state() != 3);
            }
            g.plan.active = true;
            g.plan.open   = true;
            if (g.plan.budget < 240) g.plan.budget = 900;
        }
        g.press_hit = GameHit{};
    }
}

// ── Pruefskript ───────────────────────────────────────────────────────
// WWMOD_TEST_SCRIPT=<Datei> fuehrt Regie: eine Zeile je Ereignis,
// "<Bild> <Befehl>". Damit laesst sich der ganze Weg -- Titelbild, Menue,
// Klick, geoeffneter Ordner -- ohne Hand am Zeiger fahren und belegen.
//
//   1300 key sta        eine Taste druecken (a b sta sel hoch runter links rechts)
//   2100 tap 150 152    an diesem Spielpunkt antippen (Druck, vier Bilder, los)
//   2200 hold 46 116    aufsetzen und liegen lassen
//   2260 move 46 96     den liegenden Zeiger verschieben
//   2280 up             loslassen
//   2300 wtap 640 900   dasselbe in Fensterpunkten -- dann laeuft die ganze
//                       Umrechnung mit, samt Breitbild und Drehung
//   2130 shot bild.ppm  das naechste zusammengesetzte Bild wegschreiben
//   2140 log Reiter     eine Zeile mit dem Menuezustand ausgeben
struct ScriptEvent {
    long frame = 0;
    int  kind  = 0;      // 0 key, 1 tap, 2 shot, 3 log
    uint16_t key = 0;
    int  x = 0, y = 0;
    std::string text;
};
std::vector<ScriptEvent> g_script;
long g_script_frame = 0;
std::size_t g_script_next = 0;
int  g_tap_left = 0;              // Restbilder des laufenden Tipps
int  g_tap_x = 0, g_tap_y = 0;
bool g_ptr_on = false, g_ptr_down = false;
bool g_ptr_window = false;        // wtap: Fensterpunkte statt Spielpunkte
std::string g_shot_path;

void load_script(const char* path) {
    if (!path || !path[0]) return;
    std::FILE* f = std::fopen(path, "r");
    if (!f) return;
    char line[256];
    while (std::fgets(line, sizeof(line), f)) {
        char* c = line;
        while (*c == ' ' || *c == '\t') ++c;
        if (*c == '#' || *c == '\n' || !*c) continue;
        long frame = 0; char cmd[32] = {}; char rest[192] = {};
        const int n = std::sscanf(c, "%ld %31s %191[^\n]", &frame, cmd, rest);
        if (n < 2) continue;
        ScriptEvent e;
        e.frame = frame;
        if (!std::strcmp(cmd, "key")) {
            e.kind = 0;
            char name[16] = {};
            std::sscanf(rest, "%15s", name);
            if      (!std::strcmp(name, "a"))      e.key = kKeyA;
            else if (!std::strcmp(name, "b"))      e.key = kKeyB;
            else if (!std::strcmp(name, "sta"))    e.key = kKeySta;
            else if (!std::strcmp(name, "sel"))    e.key = kKeySel;
            else if (!std::strcmp(name, "hoch"))   e.key = kKeyUp;
            else if (!std::strcmp(name, "runter")) e.key = kKeyDown;
            else if (!std::strcmp(name, "links"))  e.key = kKeyLeft;
            else if (!std::strcmp(name, "rechts")) e.key = kKeyRight;
            else continue;
        } else if (!std::strcmp(cmd, "tap")) {
            e.kind = 1;
            if (std::sscanf(rest, "%d %d", &e.x, &e.y) != 2) continue;
        } else if (!std::strcmp(cmd, "wtap")) {
            e.kind = 7;
            if (std::sscanf(rest, "%d %d", &e.x, &e.y) != 2) continue;
        } else if (!std::strcmp(cmd, "hold")) {
            e.kind = 4;
            if (std::sscanf(rest, "%d %d", &e.x, &e.y) != 2) continue;
        } else if (!std::strcmp(cmd, "move")) {
            e.kind = 5;
            if (std::sscanf(rest, "%d %d", &e.x, &e.y) != 2) continue;
        } else if (!std::strcmp(cmd, "up")) {
            e.kind = 6;
        } else if (!std::strcmp(cmd, "shot")) {
            e.kind = 2; e.text = rest;
        } else if (!std::strcmp(cmd, "log")) {
            e.kind = 3; e.text = rest;
        } else {
            continue;
        }
        g_script.push_back(e);
    }
    std::fclose(f);
    std::fprintf(stderr, "[ww-menu] Pruefskript \"%s\": %zu Ereignisse\n",
                 path, g_script.size());
}

// Ein Bild des Skripts. Rueckgabe: das Skript fuehrt gerade den Zeiger.
bool script_step(int* mx, int* my, bool* down) {
    if (g_script.empty()) return false;
    const long t = g_script_frame++;
    while (g_script_next < g_script.size() && g_script[g_script_next].frame <= t) {
        const ScriptEvent& e = g_script[g_script_next++];
        switch (e.kind) {
            case 0: queue_press(e.key); break;
            case 1: g_ptr_on = true; g_ptr_down = true; g_tap_left = 5;
                    g_ptr_window = false; g_tap_x = e.x; g_tap_y = e.y; break;
            case 7: g_ptr_on = true; g_ptr_down = true; g_tap_left = 5;
                    g_ptr_window = true; g_tap_x = e.x; g_tap_y = e.y; break;
            case 4: g_ptr_on = true; g_ptr_down = true; g_tap_left = 0;
                    g_tap_x = e.x; g_tap_y = e.y; break;
            case 5: g_tap_x = e.x; g_tap_y = e.y; break;
            case 6: g_ptr_down = false; g_tap_left = 0; break;
            case 2: g_shot_path = e.text; break;
            case 3:
                std::fprintf(stderr, "[ww-menu] Bild %ld %s -- Szene %d, "
                             "Zustand %d, Reiter %d, Eintrag %d, Zelle %d/%d, "
                             "Spalten %d, Felder %d, Versatz %d, bereit %d\n",
                             t, e.text.c_str(),
                             static_cast<int>(static_cast<int16_t>(bus_read_u16(kCurrentScene))),
                             menu_state(), menu_tab(), menu_entry(),
                             menu_var(3), menu_var(4),
                             bus_read_u8(gp(kOffGridCols)), bus_read_u8(gp(kOffGridMax)),
                             static_cast<int>(static_cast<int16_t>(bus_read_u16(kBgOfs2X))),
                             menu_ready() ? 1 : 0);
                break;
            default: break;
        }
    }
    if (g_tap_left > 0 && --g_tap_left == 0) g_ptr_down = false;
    if (!g_ptr_on) return false;
    *mx = g_tap_x; *my = g_tap_y; *down = g_ptr_down;
    return true;
}

// ── Menue per Zeiger: Logik ───────────────────────────────────────────
//
// Pfeile und Werte wirken beim Aufsetzen und wiederholen sich beim Halten.
// Was etwas oeffnet oder schliesst, loest erst beim Loslassen aus: so laesst
// sich ein Fehlgriff durch Wegziehen zuruecknehmen, und der Finger, der das
// Menue oeffnet, schliesst es nicht im selben Atemzug wieder, weil sein
// Loslassen noch dem Knopf gehoert, nicht dem Menue.
inline bool in_rect(int x0, int y0, int x1, int y1, int x, int y) {
    return x >= x0 && x < x1 && y >= y0 && y < y1;
}

int menu_row_at(int sx, int sy) {
    const MenuLayout& L = g.ml;
    if (!L.valid || sx < L.px || sx >= L.px + L.pw) return -1;
    for (int i = 0; i < kItemCount; ++i)
        if (sy >= L.row_top[i] && sy < L.row_top[i] + L.row_h) return i;
    return -1;
}

int menu_zone_at(int sx, int sy, int* row) {
    const MenuLayout& L = g.ml;
    if (row) *row = -1;
    if (!L.valid) return kUiNone;
    if (in_rect(L.close_x0, L.close_y0, L.close_x1, L.close_y1, sx, sy)) return kUiClose;
    if (!in_rect(L.px, L.py, L.px + L.pw, L.py + L.ph, sx, sy)) return kUiOutside;
    const int r = menu_row_at(sx, sy);
    if (row) *row = r;
    if (r < 0) return kUiPanel;
    if (sx >= L.val_x1[r]) return kUiArrowR;
    if (sx >= L.val_x0[r]) return kUiValue;
    if (sx >= L.arl_x0[r]) return kUiArrowL;
    return kUiRow;
}

// Ein Schritt je Durchgang. sx/sy in Flaechenkoordinaten; valid heisst, der
// Zeiger liegt auf der Flaeche. Rein -- kein SDL, damit der Selbsttest ihn
// ohne Hand am Zeiger fahren kann.
void ui_step(int sx, int sy, bool valid, bool down, bool moved) {
    UiState& u = g.ui;
    if (valid) { u.sx = sx; u.sy = sy; }
    u.valid = valid;
    const bool press   = down && !u.down && valid;
    const bool release = !down && u.down;
    u.down = down;
    if (moved || down) u.activity = kUiActivityFrames;
    else if (u.activity > 0) --u.activity;

    if (press) {
        u.hold = 0;
        u.repeats = 0;
        u.press_row = -1;
        if (!g.open) {
            const ButtonRect& b = g.mbtn;
            u.press_target = (b.visible && in_rect(b.x0, b.y0, b.x1, b.y1, sx, sy))
                                 ? kUiMenuButton : kUiGame;
        } else {
            int row = -1;
            u.press_target = menu_zone_at(sx, sy, &row);
            u.press_row = row;
            if (row >= 0) {
                g.cursor = row;
                if (u.press_target == kUiArrowR || u.press_target == kUiValue)
                    adjust(row, +1, false);
                else if (u.press_target == kUiArrowL)
                    adjust(row, -1, false);
            }
        }
    } else if (down && (u.press_target == kUiArrowL || u.press_target == kUiArrowR)) {
        int row = -1;
        if (valid && menu_zone_at(sx, sy, &row) == u.press_target && row == u.press_row) {
            // Dieselbe Wiederholrate wie auf dem Steuerkreuz; nach zwanzig
            // Wiederholungen springt es grob, wie L/R.
            if (++u.hold > 18 && (u.hold % 3) == 0) {
                ++u.repeats;
                adjust(u.press_row, (u.press_target == kUiArrowR) ? +1 : -1,
                       u.repeats > 20);
            }
        } else {
            u.hold = 0;   // weggezogen: die Wiederholung ruht
        }
    } else if (release) {
        int row = -1;
        const int zone = g.open ? menu_zone_at(u.sx, u.sy, &row) : kUiNone;
        switch (u.press_target) {
            case kUiMenuButton: {
                const ButtonRect& b = g.mbtn;
                if (!g.open && b.visible && in_rect(b.x0, b.y0, b.x1, b.y1, u.sx, u.sy)) {
                    g.open = true;
                    menu_opened();
                }
                break;
            }
            case kUiClose:
                if (g.open && zone == kUiClose) { g.open = false; menu_closed(); }
                break;
            case kUiOutside:
                if (g.open && zone == kUiOutside) { g.open = false; menu_closed(); }
                break;
            default:
                break;
        }
        u.press_target = kUiNone;
        u.press_row = -1;
    }

    // Maus ohne gedrueckte Taste: die Zeile unter dem Zeiger hervorheben. Nur
    // bei echter Bewegung -- eine ruhende Maus soll dem Steuerkreuz nicht die
    // Auswahl wegnehmen.
    if (g.open && moved && !down && valid) {
        const int row = menu_row_at(sx, sy);
        if (row >= 0) g.cursor = row;
    }
}

}  // namespace

// ── Naht ──────────────────────────────────────────────────────────────
extern "C" void ww_mod_init(void) {
    if (const char* v = std::getenv("WWMOD_RANDOM")) {
        if (!std::strcmp(v, "games") || !std::strcmp(v, "microgames"))
            g.random_mode = kRandomGames;
        else if (!std::strcmp(v, "all") || !std::strcmp(v, "alles"))
            g.random_mode = kRandomAll;
    }
    if (const char* v = std::getenv("WWMOD_SEED")) {
        const int n = std::atoi(v);
        if (n >= 1 && n <= 9999) g.seed = n;
    }
    if (const char* v = std::getenv("WWMOD_SPEED")) {
        const int n = std::atoi(v);
        if (n >= 50 && n <= 300) g.speed_pct = (n / 10) * 10;
    }
    if (const char* v = std::getenv("WWMOD_MUSIC_VOLUME")) g.music_vol = clampi(std::atoi(v), 0, 100);
    if (const char* v = std::getenv("WWMOD_SOUND_VOLUME")) g.sfx_vol   = clampi(std::atoi(v), 0, 100);
    if (const char* v = std::getenv("WWMOD_ROTATION")) g.rotation = (std::atoi(v) / 90) & 3;
    if (const char* v = std::getenv("WWMOD_SCALING")) {
        if (!std::strcmp(v, "fit"))          g.screen = kScreenFit;
        else if (!std::strcmp(v, "stretch")) g.screen = kScreenStretch;
        else                                 g.screen = kScreenSharp;
    }
    if (std::getenv("WWMOD_BLACK_BACKDROP")) g.backdrop = kBackdropBlack;
    if (const char* v = std::getenv("WWMOD_WIDESCREEN")) {
        if (!std::strcmp(v, "auto")) g.wide = kWideAuto;
        else if (!std::strcmp(v, "16:9"))  g.wide = kWide16x9;
        else if (!std::strcmp(v, "16:10")) g.wide = kWide16x10;
        else if (!std::strcmp(v, "21:9"))  g.wide = kWide21x9;
        else if (!std::strcmp(v, "max"))   g.wide = kWideMax;
        else {
            const int n = std::atoi(v);
            for (int i = 0; i < kWideMax + 1; ++i)
                if (kWideSteps[i] == n) g.wide = i;
        }
    }
    if (std::getenv("WWMOD_INFINITE_LIVES")) g.inf_lives = true;
    if (std::getenv("WWMOD_SKIP_CUTSCENES")) g.skip_scenes = true;
    if (const char* v = std::getenv("WWMOD_TOUCH")) {
        if (!std::strcmp(v, "drag"))       g.touch_mode = kTouchModeDrag;
        else if (!std::strcmp(v, "flick")) g.touch_mode = kTouchModeFlick;
    }
    load_touch_table(std::getenv("WWMOD_TOUCH_TABLE"));
    load_script(std::getenv("WWMOD_TEST_SCRIPT"));
    if (std::getenv("WWMOD_KAIZO")) g.kaizo = true;
    if (std::getenv("WWMOD_BOSS_RUSH")) g.boss_rush = true;
    if (std::getenv("WWMOD_READOUT")) g.readout = true;
    if (std::getenv("WWMOD_NO_MENU_MUSIC")) g.menu_music = false;
    if (std::getenv("WWMOD_NO_GUEST_CALL")) g.guest_call_ok = false;

    g.player_count = clampi(bus_read_u8(kSoundPlayerCount), 0, kMaxSoundPlayers);

    g_prev_entry_hook = g_runtime_fn_entry_hook;
    g_runtime_fn_entry_hook = &entry_hook;

    std::fprintf(stderr,
                 "[ww-mod] ready -- F10 opens the menu "
                 "(randomizer=%s seed=%d speed=%d%% players=%d)\n",
                 random_label(g.random_mode), g.seed, g.speed_pct, g.player_count);
}

// Welches Stueck der Haupt-BGM-Spieler gerade geladen hat -- nur zum
// Mitschreiben.
int current_bgm_song() {
    const uint32_t sp = bus_read_u32(kSoundPlayerTable);
    if (!is_ram(sp)) return -1;
    const uint32_t song = bus_read_u32(sp + kOffPlayerSong);
    if (!is_rom(song)) return -1;
    return static_cast<int>(bus_read_u32(song + kOffSongNum));
}

extern "C" void ww_mod_frame(void) {
    tick_effects();

    // WWMOD_TEST_MENU=<n>: oeffnet das Menue nach n Bildern und schliesst es
    // 300 Bilder spaeter. Damit ist der ganze Weg -- Spielpause per
    // eingeschobenem Start, Gastaufruf fuer die Menuemusik, Zeichnen,
    // Rueckkehr -- auch ohne Tastatur belegbar.
    static const long test_at = [] {
        const char* e = std::getenv("WWMOD_TEST_MENU");
        return e ? std::atol(e) : -1L;
    }();
    if (test_at >= 0) {
        static long n = 0;
        const long t = n++;
        if (t == test_at || t == test_at + 300) {
            const bool opening = (t == test_at);
            g.open = opening;
            if (opening) menu_opened(); else menu_closed();
            std::fprintf(stderr,
                         "[ww-mod] Test: Menue %s bei Bild %ld "
                         "(Szene %d, Zustand %d, BGM %d)\n",
                         opening ? "auf" : "zu", t,
                         static_cast<int>(static_cast<int16_t>(bus_read_u16(kCurrentScene))),
                         gameplay_state(), current_bgm_song());
        }
        if (t == test_at + 30 || t == test_at + 330)
            std::fprintf(stderr, "[ww-mod] Test: 30 Bilder spaeter -- "
                         "Zustand %d, BGM %d\n", gameplay_state(), current_bgm_song());
    }

    if (g.pending_song >= 0) {
        const int song = g.pending_song;
        g.pending_song = -1;
        const bool ok = call_guest(kFnSongStart, static_cast<uint32_t>(song));
        static const bool trace = std::getenv("WWMOD_TRACE") != nullptr;
        if (trace)
            std::fprintf(stderr, "[ww-mod] Gastaufruf Stueck %d: %s (BGM jetzt %d)\n",
                         song, ok ? "zurueck" : "ABGEBROCHEN", current_bgm_song());
    }
}

extern "C" uint16_t ww_mod_keys(uint16_t keyinput) {
    const uint16_t k = keyinput;
    const uint16_t prev = g.prev_keys;
    g.prev_keys = k;

    // Den Zeiger genau einmal je Durchgang lesen; Menue und Spiel teilen sich
    // den Zustand. Ein Druck gehoert dem, was er beim Aufsetzen trifft.
    int wx = 0, wy = 0;
    bool pdown = false;
    const bool have = poll_pointer(&wx, &wy, &pdown);
    const bool moved = have && g.ui.last_wx >= 0 &&
                       (wx != g.ui.last_wx || wy != g.ui.last_wy);
    if (have) { g.ui.last_wx = wx; g.ui.last_wy = wy; }
    int sx = 0, sy = 0;
    const bool on_surface = have && window_to_surface(wx, wy, &sx, &sy);
    const bool was_open = g.open;
    ui_step(sx, sy, on_surface, have && pdown, moved);
    if (g.open != was_open) {
        // Per Zeiger geoeffnet oder geschlossen: dieser Durchgang gehoert dem
        // Menue, das Spiel sieht nichts davon. Ein laufender Weg durch das
        // Spielmenue endet hier -- sonst liefe er hinter dem Mod-Menue weiter.
        g.pointer_was = false;
        g.touch_press_left = g.touch_dir_left = 0;
        g.plan.active = false;
        g.press_hit = GameHit{};
        g.game_ptr_down = false;
        return 0x03FFu;
    }

    // Pruefskript (WWMOD_TEST_SCRIPT): fuehrt Tasten und Zeiger, wenn es
    // geladen ist. Ohne Datei ist das hier ein Rueckgabewert.
    int smx = 0, smy = 0;
    bool sdown = false;
    const bool scripted = script_step(&smx, &smy, &sdown);

    // Laufender Tastenimpuls hat Vorrang -- er gehoert dem Spiel, nicht dem
    // Menue. Erst gedrueckt, dann losgelassen, damit die Flanke ankommt.
    if (g.inject_left > 0) {
        const int n = g.inject_left--;
        return (n > 3) ? static_cast<uint16_t>(0x03FFu & ~g.inject_bits) : 0x03FFu;
    }

    // Oeffnen/Schliessen: F10 oder die Tastenkombination L+R+Select
    // (damit es auch ohne Tastatur erreichbar bleibt).
    const bool combo      = down(k, kKeyL) && down(k, kKeyR) && down(k, kKeySel);
    const bool combo_prev = down(prev, kKeyL) && down(prev, kKeyR) && down(prev, kKeySel);
    if (f10_edge() || (combo && !combo_prev)) {
        g.open = !g.open;
        if (g.open) menu_opened(); else menu_closed();
        return 0x03FFu;
    }

    if (!g.open) {
        // Ans Spiel geht nur ein Druck, der auch dort aufgesetzt hat -- nicht
        // der auf dem Menueknopf.
        const bool game_down = pdown && g.ui.press_target == kUiGame;

        // Im Hauptmenue des Spiels bedient der Zeiger die Menues selbst: dort
        // waere ein erfundenes A an der Zeigerstelle sinnlos, weil das Spiel
        // gar nicht weiss, wo der Zeiger steht.
        if (in_main_menu()) {
            int gx = 0, gy = 0;
            const bool on_game = have && window_to_game(wx, wy, &gx, &gy);
            g.ptr_valid = on_game;
            if (on_game) { g.ptr_gx = gx; g.ptr_gy = gy; }
            g.pointer_was = false;
            g.touch_press_left = g.touch_dir_left = 0;
            // Die Ansicht kann breiter als 240 Punkte sein; das Spiel rechnet
            // weiter in seinen eigenen, also den linken Rand abziehen.
            const int off = (g.geo_gw - 240) / 2;
            int mx = gx - off, my = gy;
            bool mdown = game_down, mvalid = on_game;
            if (scripted) {
                mdown = sdown; mvalid = true;
                if (g_ptr_window) {
                    // Fensterpunkt: dieselbe Umrechnung wie beim echten Zeiger.
                    int tx = 0, ty = 0;
                    mvalid = window_to_game(smx, smy, &tx, &ty);
                    mx = tx - off; my = ty;
                    g.ptr_valid = mvalid;
                    if (mvalid) { g.ptr_gx = tx; g.ptr_gy = ty; }
                } else {
                    mx = smx; my = smy;
                    // Damit das Fadenkreuz im Pruefbild dort steht, wo das
                    // Skript tippt.
                    g.ptr_valid = true; g.ptr_gx = mx + off; g.ptr_gy = my;
                }
            }
            game_menu_pointer(mx, my, mvalid, mdown);
            // Wer selbst zur Tastatur greift, bricht den Plan ab.
            if (keyinput != 0x03FFu) g.plan.active = false;
            const uint16_t bits = plan_step();
            if (bits) {
                g.last_menu_key = bits;
                queue_press(bits);
            }
            if (g.inject_left > 0) {
                const int n = g.inject_left--;
                return (n > 3) ? static_cast<uint16_t>(0x03FFu & ~g.inject_bits)
                               : 0x03FFu;
            }
            return keyinput;
        }

        // Der Zeiger ergaenzt die Tastatur, er ersetzt sie nicht: was die
        // Hand auf den Tasten haelt, bleibt gedrueckt.
        uint16_t syn = 0;
        const int scheme = touch_scheme();
        if (scheme != kTouchOff && have) {
            syn = pointer_keys_from(wx, wy, game_down, scheme);
        } else {
            g.ptr_valid = false;
            g.pointer_was = false;
        }
        return static_cast<uint16_t>(keyinput & ~syn);
    }
    g.pointer_was = false;
    g.touch_press_left = g.touch_dir_left = 0;

    // Wiederholrate: erst Flanke, nach kurzem Halten automatisch weiter.
    auto fired = [&](int bit) -> bool {
        const bool now = down(k, bit), was = down(prev, bit);
        if (now && !was) { g.hold_frames = 0; g.held_dir = bit; return true; }
        if (now && g.held_dir == bit) {
            ++g.hold_frames;
            if (g.hold_frames > 18 && (g.hold_frames % 3) == 0) return true;
        }
        return false;
    };

    if (fired(kKeyUp))    g.cursor = (g.cursor + kItemCount - 1) % kItemCount;
    if (fired(kKeyDown))  g.cursor = (g.cursor + 1) % kItemCount;
    if (fired(kKeyLeft))  adjust(g.cursor, -1, false);
    if (fired(kKeyRight)) adjust(g.cursor, +1, false);
    if (fired(kKeyL))     adjust(g.cursor, -1, true);
    if (fired(kKeyR))     adjust(g.cursor, +1, true);
    if (down(k, kKeyA) && !down(prev, kKeyA)) adjust(g.cursor, +1, false);
    if (down(k, kKeySta) && !down(prev, kKeySta)) { g.open = false; menu_closed(); }
    if (!down(k, kKeyUp) && !down(k, kKeyDown) && !down(k, kKeyLeft) &&
        !down(k, kKeyRight) && !down(k, kKeyL) && !down(k, kKeyR)) {
        g.hold_frames = 0;
        g.held_dir = 0;
    }
    return 0x03FFu;  // der Gast sieht nichts davon
}

// ── Darstellung ───────────────────────────────────────────────────────
extern "C" int ww_mod_view_width(void) { return wide_width(); }

extern "C" int ww_mod_owns_view(void) {
    static const int on = (std::getenv("WWMOD_NO_VIEW") == nullptr) ? 1 : 0;
    return on;
}

extern "C" int ww_mod_surface_size(int win_w, int win_h, int* out_w, int* out_h) {
    if (!out_w || !out_h || win_w < 1 || win_h < 1) return 0;
    // Die Flaeche ist das Fenster, gedeckelt. Damit steht das Spielbild in
    // ganzzahligem Massstab darin und der letzte Schritt zum Fenster ist 1:1
    // (oder bei sehr grossen Anzeigen ein glatter Faktor).
    constexpr int kMaxW = 1920, kMaxH = 1200;
    int w = win_w, h = win_h;
    if (w > kMaxW) { h = static_cast<int>(static_cast<long long>(h) * kMaxW / w); w = kMaxW; }
    if (h > kMaxH) { w = static_cast<int>(static_cast<long long>(w) * kMaxH / h); h = kMaxH; }
    // Nie kleiner als das gedrehte Spielbild, sonst muesste es beschnitten
    // werden, bevor ueberhaupt skaliert wird. Im Breitbild ist das Spielbild
    // breiter als 240.
    g.win_w = win_w;
    g.win_h = win_h;
    const bool swap = (g.rotation & 1) != 0;
    const int base_w = wide_width();
    const int need_w = swap ? 160 : base_w;
    const int need_h = swap ? base_w : 160;
    if (w < need_w) w = need_w;
    if (h < need_h) h = need_h;
    *out_w = w; *out_h = h;
    return 1;
}

extern "C" void ww_mod_compose(const uint8_t* game, int gw, int gh,
                               uint8_t* out, int ow, int oh) {
    if (!game || !out || gw < 1 || gh < 1 || ow < 1 || oh < 1) return;
    const size_t gbytes = static_cast<size_t>(gw) * gh * 3;
    const size_t obytes = static_cast<size_t>(ow) * oh * 3;

    // Bei offenem Menue das Spielbild einfrieren -- das Spiel steht ohnehin
    // in seiner eigenen Pause, aber so bleibt auch der letzte Rest Bewegung
    // (Blinken, Tonanzeige) stehen.
    if (g.open) {
        if (g.frozen.size() != gbytes || g.frozen_w != gw || g.frozen_h != gh) {
            g.frozen.assign(game, game + gbytes);
            g.frozen_w = gw; g.frozen_h = gh;
        }
        game = g.frozen.data();
    }

    // Hintergrund
    if (g.backdrop == kBackdropBlack) {
        std::memset(out, 0, obytes);
    } else {
        if (g.ambient.size() != obytes || g.ambient_w != ow || g.ambient_h != oh) {
            g.ambient.assign(obytes, 0);
            g.ambient_w = ow; g.ambient_h = oh;
            g.ambient_age = 0;
            build_ambient(game, gw, gh, g.ambient.data(), ow, oh);
        } else if (!g.open && ++g.ambient_age >= kAmbInterval) {
            g.ambient_age = 0;
            build_ambient(game, gw, gh, g.ambient.data(), ow, oh);
        }
        std::memcpy(out, g.ambient.data(), obytes);
    }

    // Spielbild
    const bool swap = (g.rotation & 1) != 0;
    const int rw = swap ? gh : gw;
    const int rh = swap ? gw : gh;
    int dw = rw, dh = rh;
    if (g.screen == kScreenStretch) {
        dw = ow; dh = oh;
    } else if (g.screen == kScreenFit) {
        const long long by_w = static_cast<long long>(ow) * rh;
        const long long by_h = static_cast<long long>(oh) * rw;
        if (by_w < by_h) { dw = ow; dh = static_cast<int>(by_w / rw); }
        else             { dh = oh; dw = static_cast<int>(by_h / rh); }
    } else {
        int k = ow / rw;
        const int ky = oh / rh;
        if (ky < k) k = ky;
        if (k < 1) k = 1;
        dw = rw * k; dh = rh * k;
    }
    const int ddx = (ow - dw) / 2, ddy = (oh - dh) / 2;
    blit_game(game, gw, gh, out, ow, oh, ddx, ddy, dw, dh, g.rotation);

    // Geometrie festhalten: ww_mod_keys rechnet damit den Fensterpunkt
    // zurueck auf einen Spielpunkt.
    g.geo_sw = ow; g.geo_sh = oh;
    g.geo_dx = ddx; g.geo_dy = ddy; g.geo_dw = dw; g.geo_dh = dh;
    g.geo_gw = gw; g.geo_gh = gh; g.geo_rot = g.rotation;

    // Bedienoberflaeche. Der Massstab folgt der Flaeche, damit die Schrift
    // auf jeder Fenstergroesse gleich gross wirkt -- und bleibt klein genug,
    // dass das Menue auch in ein schmales Hochkantfenster passt.
    int sc = oh / 220;
    if (sc < 1) sc = 1;
    if (sc > 6) sc = 6;
    // WWMOD_DUMP_COMPOSE=<Pfad> schreibt den ersten zusammengesetzten Rahmen
    // als PPM -- damit ist der Weg Flaeche/Zusammensetzen/Praesentieren auch
    // ohne Bildschirm belegbar (SDL_VIDEODRIVER=dummy).
    static const char* dump = std::getenv("WWMOD_DUMP_COMPOSE");
    static const unsigned long dump_after = [] {
        const char* e = std::getenv("WWMOD_DUMP_AFTER");
        return e ? std::strtoul(e, nullptr, 10) : 0ul;
    }();
    static unsigned long compose_calls = 0;
    const bool dump_now = dump && (compose_calls++ == dump_after);

    if (g.touch_mode != kTouchModeOff && !g.open) draw_pointer(out, ow, oh, sc);
    if (g.readout) draw_readout(out, ow, oh, sc);
    draw_menu_button(out, ow, oh, sc);
    if (!g.open) g.ml.valid = false;
    if (g.open) {
        const int msc = menu_scale(ow, oh);
        draw_menu(out, ow, oh, msc < sc ? msc : sc);
    }

    if (!g_shot_path.empty()) {
        if (std::FILE* f = std::fopen(g_shot_path.c_str(), "wb")) {
            std::fprintf(f, "P6\n%d %d\n255\n", ow, oh);
            std::fwrite(out, 1, obytes, f);
            std::fclose(f);
            std::fprintf(stderr, "[ww-menu] Bild %dx%d -> %s\n", ow, oh,
                         g_shot_path.c_str());
        }
        g_shot_path.clear();
    }

    if (dump_now) {
        if (std::FILE* f = std::fopen(dump, "wb")) {
            std::fprintf(f, "P6\n%d %d\n255\n", ow, oh);
            std::fwrite(out, 1, obytes, f);
            std::fclose(f);
            std::fprintf(stderr, "[ww-mod] composed frame %dx%d -> %s\n", ow, oh, dump);
        }
    }
}
