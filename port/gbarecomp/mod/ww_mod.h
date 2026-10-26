// ww_mod.h -- Naht zwischen gbarecomp und den WarioWare-Mods.
//
// gbarecomp gehoert uns nicht: der Klon unter ../../gbarecomp/ wird beim
// Aktualisieren ueberschrieben. Deshalb liegt in runtime.cpp nur diese Naht
// -- sechs Aufrufe gegen schwach definierte No-Ops -- und der gesamte
// Mod-Code hier. Ohne mitgelinkten Mod verhaelt sich die Laufzeit exakt wie
// vorher; der Patch dazu ist mod-seam.patch.
//
// Der Gastzustand wird ueber bus_read_*/bus_write_* aus runtime_arm.h
// gelesen und geschrieben, Gastfunktionen ueber g_runtime_fn_entry_hook
// abgefangen. Es wird kein einziges Byte der ROM veraendert: die Mods
// greifen zur Laufzeit in RAM und Registerdatei, nie in den Code.

#pragma once

#include <stdint.h>

extern "C" {

// Einmal beim Start, nachdem Bus und PPU stehen. Haengt den Funktions-
// eintrittshaken ein und liest die Umgebungsvariablen.
void ww_mod_init(void);

// Einmal je praesentiertem Bild, vor dem Zusammensetzen. Haelt Dauer-
// wirkungen nach (Lautstaerken, unendliche Leben) und stoesst aufgeschobene
// Gastaufrufe an.
void ww_mod_frame(void);

// Sieht die Tasten vor dem Gast. Bei offenem Menue schluckt der Mod sie und
// schiebt die Impulse ein, mit denen das Spiel in seine eigene Pause geht.
uint16_t ww_mod_keys(uint16_t keyinput);

// ── Darstellung ───────────────────────────────────────────────────────
// !=0 heisst: der Mod setzt das Bild selbst zusammen. Die Laufzeit oeffnet
// dann ein frei skalierbares Fenster und laesst die Zeichenflaeche seiner
// Groesse folgen.
int ww_mod_owns_view(void);

// Gewuenschte Breite der PPU-Ansicht in Punkten: 240 = originalgetreu,
// mehr = echtes Breitbild. Die Laufzeit stellt ppu.set_view_margins()
// entsprechend um, sobald sich der Wert aendert.
int ww_mod_view_width(void);

// Welche Zeichenflaeche der Mod fuer ein Fenster dieser Groesse moechte.
// !=0 = uebernehmen; *out_w/*out_h in Bildpunkten.
int ww_mod_surface_size(int win_w, int win_h, int* out_w, int* out_h);

// Setzt das fertige Bild zusammen: Hintergrund, Spielbild (gedreht und
// skaliert), HUD, Menue. `game` ist der rohe PPU-Rahmen in RGB888.
void ww_mod_compose(const uint8_t* game, int gw, int gh,
                    uint8_t* out, int ow, int oh);

}  // extern "C"
