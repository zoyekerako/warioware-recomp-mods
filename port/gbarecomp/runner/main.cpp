// WarioWare-Runner fuer gbarecomp.
//
// gbarecomp::run_game() besitzt den gesamten Lebenszyklus -- ROM laden und
// pruefen, BIOS laden, CPU/Bus/PPU aufsetzen, Scheduler, Hauptschleife. Ein
// Spiel-Ziel muss nur seine Identitaet mitbringen und die generierten Shards
// mitlinken.
//
// Das ist NICHT der Port unter ../../ -- hier laeuft maschinenuebersetzter
// Code gegen ein Hardwaremodell, dort byte-genau dekompilierter Quelltext.

#include "runtime.h"

int main(int argc, char** argv) {
    gbarecomp::RunOptions opts;

    opts.builtin_game_name = "WarioWare Inc.: Mega Microgame$";
    opts.builtin_rom_sha1  = "3f556448d290fa5406d6ed367fee16cc02387ad3";
    // Echtes Breitbild. WarioWares Hintergrundkarten wiederholen sich in den
    // Zusatzspalten, weil dort kein Inhalt komponiert ist -- deshalb steht im
    // Mod-Menue "(May have bugs)" daneben. Bis 480 Punkte breit, das ist
    // gleichzeitig die Grenze der Laufzeit (GbaPpu::kMaxRenderWidth).
    opts.max_view_width = 480;
    // Der Umschalter sitzt im Mod-Menue, nicht im Startfenster.
    opts.launcher_expose_widescreen = false;

    // opts MUSS uebergeben werden. Bis hierher tat es das nicht -- damit waren
    // Spielname, ROM-Pruefsumme und die Breitbild-Freigabe allesamt wirkungslos,
    // und --view-width lief in "this game has not opted in".
    return gbarecomp::run_game(argc, argv, opts);
}
