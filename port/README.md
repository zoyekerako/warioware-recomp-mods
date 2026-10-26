# WarioWare Inc. — PC-Port, Werkzeuge

Arbeitsverzeichnis für den Port. Das Decomp-Repo unter `../wariowareinc-main`
bleibt unberührt; hier liegt nur, was der Port selbst braucht.

## Werkzeuge

Nachschlagen, was es gibt:

| Werkzeug | Zweck |
|----------|-------|
| `verify_file.py` | eine Einheit gegen das ROM prüfen |
| `verify_all.sh` | alle Einheiten prüfen — nach jeder Header-Änderung |
| `new_unit.py` | Übersetzungseinheit aus loser Assembly schneiden |
| `offsets.py` | Struktur-Offsets, wie der GBA-Compiler sie legt |
| `gen_pc_stubs.py` | Stub-Datei für alles noch Fehlende erzeugen |
| `call_graph.py` | transitive Hülle über Assembly- und C-Aufrufe |
| `scan_struct.py`, `scan_param.py`, `bitfield_report.py`, `gen_struct.py` | Strukturen aus der Disassembly rekonstruieren |
| `bs2c.py` | Beatscript nach C, mit Rückübersetzungs-Probe |
| `rom2c.py` | ROM-Rohblock nach C, mit typisierten Zeigertabellen |
| `extract_gfx.py`, `extract_gfx_tables.py`, `convert_anim.py`, `preview_4bpp.py` | Grafik-Pipeline |

### `tools/extract_gfx.py`

Scannt eine 8-MiB-ROM nach `CompressedData`-Deskriptoren und dekodiert jedes
gefundene Asset. Der Codec ist **kein** BIOS-LZ77, sondern hauseigen und
zweistufig — Details im Modul-Docstring.

```
python3 tools/extract_gfx.py ../rom.gba -o assets/gfx
```

Ergebnis: 1038 Assets, 6.733.278 Byte, plus `manifest.json` mit ROM-Adresse,
Größe und Kompressionsmodus je Eintrag. Laufzeit ~9 s.

Dateiendungen: `.4bpp` für zweistufig komprimierte Assets (laut Build-Regel
Tilesets), `.rle.bin` für einstufige — der Deskriptor allein unterscheidet
nicht zwischen Tilemaps und `.rle.4bpp`-Tilesets.

### `tools/extract_gfx_tables.py`

Findet die `GraphicsTable`-Arrays (12-Byte-Records `{src, dest, size}`,
nullterminiert). `dest` sagt, *was* ein Asset ist — das kann der
`CompressedData`-Deskriptor allein nicht.

```
python3 tools/extract_gfx_tables.py ../rom.gba \
        -o assets/graphics_tables.json --min-entries 1 --extract-palettes assets/pal
```

Ergebnis: 580 Tabellen, 2742 Einträge (1682 komprimiert, 11 Loader-Funktionen,
1049 Rohkopien), 335 Palettenblobs.

**Zwei Annahmen über `dest`, die falsch sind und teuer werden:**

1. Paletten-Ziele sind *nicht* zwingend bank-ausgerichtet. Das Spiel patcht auch
   einzelne Farben mitten in einer Bank (8 Byte = 4 Farben bei +0x10). Wer
   32-Byte-Ausrichtung erzwingt, bricht ganze Tabellenläufe ab und verliert rund
   zwei Drittel aller Einträge.
2. `dest == NULL` ist für `FUNCTION_GFX_SOURCE` (`size == -2`) gültig — ein
   eigener Loader berechnet sein Ziel selbst. Verwechslung mit dem Terminator
   `{0,0,0}` ist ausgeschlossen, weil der `size == 0` hat.

Da ein Tabellenlauf beim ersten unplausiblen Eintrag abbricht, kostet jede zu
enge Annahme nicht nur den Eintrag, sondern den Rest der Tabelle.

### `tools/find_gfx_table_refs.py`

Liest die Argumente der Beatscript-Opcodes `0x2D load_graphics` und
`0x3E load_graphics_async` — ohne den Interpreter zu dekompilieren, denn nur
der Parameter wird gelesen.

```
python3 tools/find_gfx_table_refs.py ../rom.gba -o assets/beatscript_table_refs.txt
```

Ergebnis: 294 Aufrufstellen, 283 eindeutige Tabellenadressen im Bereich
0x083A8D10..0x083CA400. 268 davon findet der Blindscan ohnehin; die Liste dient
als `--roots`-Absicherung.

### `tools/preview_4bpp.py`

Rendert rohe 4bpp-Kacheldaten als PNG zur Sichtprüfung. Nur Standardbibliothek,
keine Abhängigkeiten.

```
python3 tools/preview_4bpp.py assets/gfx/gfx_08544A98.4bpp out.png \
        --palette assets/pal/title.pal --bank 0 --width 32
```

Ohne `--palette` wird eine Graustufenrampe benutzt, Farbindex 0 in Magenta,
damit Kachelgrenzen sichtbar bleiben.

## Toolchain (eingerichtet)

Für eine *matchende* Dekompilierung reicht plausibles C nicht — es muss dieselben
Bytes erzeugen wie der Originalcompiler. Das ist jetzt prüfbar:

* `arm-none-eabi-binutils` 2.47 via Homebrew.
* `agbcc` (Nintendos GCC-2.8.1-Fork) aus `pret/agbcc` gebaut, installiert nach
  `../wariowareinc-main/tools/agbcc/`.

Zwei Stolpersteine auf macOS/ARM64, die beide nicht offensichtlich sind:

1. `build.sh` reicht `$CCOPT` **ungequotet** an `make` weiter. Ein `CC` mit
   Leerzeichen (etwa clang plus Warnungsflags) zerfällt dadurch in make-Optionen
   und der Bau bricht mit der make-Hilfeseite ab. Lösung: clang in ein
   Wrapper-Skript packen, damit `CC` ein einzelnes Wort bleibt. agbcc ist
   K&R-C von 1998 und braucht `-std=gnu89` plus abgeschaltete
   implicit-function/int-Fehler.
2. Die Homebrew-Binutils werden vom Kernel mit **SIGKILL** beendet (Exit 137),
   obwohl ihre Ad-hoc-Signatur gültig aussieht. Symptom im Bau: `Killed: 9` bei
   `arm-none-eabi-as`. Lösung: alle Mach-O-Binaries neu signieren.

```
for f in $(find /opt/homebrew/Cellar/arm-none-eabi-binutils/*/  -type f -perm -u+x); do
  file -b "$f" | grep -q Mach-O && { xattr -c "$f"; codesign -f -s - "$f"; }
done
```

### `tools/verify_file.py`

Prüft eine **ganze** Übersetzungseinheit gegen das ROM. Das ist der stärkere
Test: matcht der komplette `.text`, dann stimmen alle Funktionen darin *und*
alle Aufrufstellen zwischen ihnen. Genau das braucht man, bevor man einen
gemeinsamen Header ändert — eine Signaturänderung kann sonst still einen
Aufrufer verschieben.

```
python3 tools/verify_file.py src/lib_sprite.c 0x080EE7B4
```

### `tools/verify_func.py`

Kompiliert eine `.c` durch die agbcc-Pipeline, zieht ein Symbol aus dem Objekt
und vergleicht es mit demselben Adressbereich im ROM. Bytes an
Relokationsstellen werden getrennt gezählt, weil dort erst der Linker einsetzt.

```
python3 tools/verify_func.py ../wariowareinc-main/src/code_08000f10.c func_08000F28
```

Die Adresse wird aus dem Symbolnamen abgeleitet; für benannte Funktionen ohne
Adresse im Namen (`vblank_wait`, `dma3_set`) muss `--addr` gesetzt werden, weil
deren Lage erst aus dem Map-File eines Gesamtbaus hervorgeht.

Belegt: `func_08000F28` aus dem bereits dekompilierten `src/code_08000f10.c`
liefert **MATCH** — 0 echte Abweichungen bei 32 Byte.

### Was beim Matchen zählt

Regeln, die sich über inzwischen rund 190 byte-genaue Funktionen bewährt haben.
Die ersten drei stammen aus `lib_sprite`, der Rest kam beim Rekonstruieren der
Szenendaten-Strukturen dazu.

1. **Vorzeichen sind in der Assembly ablesbar.** `LSLS #16 / LSRS #16` ist
   Zero-Extend, also `u16`; `LSLS #16 / ASRS #16` ist Sign-Extend, also `s16`.
   Analog `#24` für 8-Bit. Ein falscher Rückgabetyp erzeugt am Funktionsende
   zwei überzählige Instruktionen und verschiebt alles danach.
2. **Header-Deklarationen sind Hypothesen.** Für noch nicht dekompilierte
   Funktionen sind die Signaturen im Header geraten. `sprite_anim_get_cel_total`
   und `sprite_get_anim_duration` waren beide als vorzeichenbehaftet deklariert
   und sind es nicht. Wer sich an den Header hält, kann nicht matchen.
3. **Erst disassemblieren, dann C schreiben.** Die C-Formulierung zu variieren,
   ohne die erzeugte Assembly gegen das Original zu legen, ist Raten — und hat
   bei `func_080EFA60` das Ergebnis verschlechtert statt verbessert.
4. **Undeklarierte Funktionen konvertieren ihre Argumente nicht.** Wo kein
   Prototyp sichtbar ist, findet am Aufrufort keine Umwandlung statt — das ROM
   kürzt trotzdem, also muss der Cast von Hand geschrieben werden:
   `list[(u16)get_random_range((u16)count)]`. Das Gegenstück: wo ein Prototyp
   sichtbar ist, ist er verbindlich, und ein zusätzlicher Cast erzeugt nichts.
5. **`MOVS #k; RSBS Rn,Rn,#0` markiert eine Bitfeld-Zuweisung.** Die Löschmaske
   `~(k-1)` wird in `int`-Breite gerechnet und passt nicht in ein Thumb-Immediate.
   Von Hand geschriebene Masken faltet agbcc dagegen zu einem Literal — das ist
   der Unterschied, an dem diese Funktionsklasse lange gescheitert ist.
6. **Bitfeld-Zugriffe verengen sich auf die kleinste enthaltende Einheit.** Ein
   `u32 x:1` auf Bit 26 erscheint als `LDRB`/`STRB` auf Byte 3 der Einheit. Aus
   den rohen Offsets ist die Einheitengrenze deshalb nicht ablesbar — nur aus
   den Masken. `LSLS #a; LSRS #b` liest ein Feld ab Bit `b-a` der Breite `32-b`.
7. **Ein 16-Bit-Parameter auf dem Stack wird ganz geladen.** Ein als `s32`
   deklarierter Parameter, der nur als Halbwort gespeichert wird, verengt agbcc
   zu `LDRH` direkt aus dem Argumentslot; ein 16-Bit-Parameter erzeugt ein
   volles `LDR` in ein Register. Das ROM verrät damit die Parameterbreite.
8. **Die Vorzeichenwahl eines 16-Bit-Parameters sieht man am Aufrufer.** Ein
   `u16`-Wert an einen `s16`-Parameter erzeugt `LSLS/ASRS`, an einen
   `u16`-Parameter gar nichts. Im Callee sieht man sie nicht: die
   Eintritts-Normalisierung richtet sich nach dem Ziel der ersten Zuweisung,
   nicht nach dem deklarierten Vorzeichen.
9. **Eine lokale Kopie der Array-Basis erzwingt das Hoisting.**
   `struct E *base = D_083AA0C4;` legt die Basis in ein Callee-saved-Register;
   ohne die lokale Variable lädt agbcc sie in jeder Iteration neu.
10. **Ein Ausstieg mitten in `while (1)` verhindert das Duplizieren des
    Schleifentests.** Steht die Bedingung oben, kopiert agbcc sie als Wächter
    vor die Schleife und erzeugt zwei Kopien des Rumpfanfangs. Das ROM zeigt in
    solchen Fällen nur eine — dann gehört der Ausstieg in die Mitte.
11. **`ASRS #1` ist `>> 1`, nicht `/ 2`.** Eine vorzeichenbehaftete Division
    durch zwei erzeugt zusätzlich die Rundungskorrektur `ADD Rn, Rn, LSR #31`.
12. **Case-Rümpfe stehen in Quelltextreihenfolge.** Die Sprungtabelle ist nach
    Fallwert sortiert, die Rümpfe dahinter nicht — ihre Reihenfolge im ROM
    verrät, in welcher Reihenfolge die `case`-Zweige geschrieben wurden.
13. **Eine benannte Zwischenvariable zieht ihre Ladeoperation nach vorn.** Wo
    das ROM erst rechnet und dann dereferenziert, steht im Quelltext ein
    einziger Ausdruck; wo es vor einem Aufruf lädt, steht eine lokale Variable.
14. **Der Ort der Parameter-Normalisierung verrät, wo die Verengung steht.**
    Ganz am Funktionsanfang heißt: der Parameter ist selbst 16 Bit breit.
    Erst später, mitten im Rumpf: der Parameter ist breit und der Cast steht
    im Ausdruck.
15. **Zwei Ladeoperationen desselben Wertes trennen die Schleifenform.** Lädt
    das ROM den Bedingungswert zweimal in getrennten Blöcken, gehört die
    Bedingung in den `while`-Kopf; lädt es ihn einmal, gehört der Ausstieg in
    die Mitte von `while (TRUE)`.
16. **Die Deklarationsreihenfolge zweier Variablen mit gleichem Startwert
    entscheidet das Zielregister.** Das ROM lädt in das Register der zuerst
    deklarierten Variablen und kopiert von dort.
17. **Ein neuer Rückgabetyp ändert die Aufrufer.** Eine Funktion, die vorher
    nur implizit deklariert war, galt als `int`; wird sie als `u8` dekompiliert,
    verschiebt sich der Code jedes Aufrufers. Nach jeder neuen Signatur die
    ganze Übersetzungseinheit prüfen, nicht nur die neue Funktion.

### Szenendaten-Strukturen

`gCurrentSceneData` ist ein `void *`; jede Szene legt ihre eigene Struktur über
denselben IWRAM-Puffer (`D_03003860`, von `fill_memory_dma` mit 0x35C Worten
geleert). Rekonstruiert wurden:

| Struktur | Datei | Umfang |
|---|---|---|
| `IntroSceneData` | `src/scenes/intro.h` | 0xA0, 16 belegte Offsets |
| `SoundIdMapping` | `src/beatscript.h` | 0x8, Paartabelle |
| `Unk03000010` | `src/code_08001a70.h` | 0x8 |
| `Unk08547E78` | `src/scenes/name_select.h` | 0x4 |
| `MainMenuSceneData` | `src/scenes/main_menu.h` | 0x1F8, 92 belegte Offsets |
| `NameSelectSceneData` | `src/scenes/name_select.h` | 0x4F8, 69 belegte Offsets |
| `SceneObjectDef` | `src/scene.h` | 0x10, Objektliste einer Szene |
| `SceneSpriteMotion*` | `src/scene.h` | vier Varianten, 0xC bis 0x12 |
| `GfxTableTask` | `src/graphics_table.h` | 0x5C |

Erzeugt wurden sie aus dem Disassembler-Bestand, nicht geraten:
`tools/scan_struct.py` verfolgt einen Basiszeiger durch alle Register und
protokolliert jeden Zugriff mit Breite und Vorzeichen; `tools/scan_param.py`
macht dasselbe für Strukturen, die nur über ein Funktionsargument erreichbar
sind; `tools/bitfield_report.py` leitet Bitpositionen und -breiten aus den
Masken ab; `tools/gen_struct.py` gießt das Ergebnis in eine C-Deklaration, in
der jeder unberührte Bereich ein benanntes Pad bleibt.

Jede Struktur wurde gegen bereits byte-genauen Code geprüft, bevor neue
Funktionen darauf aufgebaut wurden — `MainMenuSceneData` etwa reproduziert nach
dem Umbau dieselben 23856 Byte wie vorher.

### Bekannte Grenzen

Drei Funktionen bleiben als Assembly stehen, alle aus demselben Grund —
Registerzuteilung und Blocklayout, nicht Semantik:

* `scene_set_sprite_motion_sine_vel` (`src/scene.c`)
* `func_0800BEC0` (`src/bitmap_font.c`) — das ROM teilt den Wertebereich bei 3,
  agbcc prüft zuerst die untere Grenze.
* `func_080135E8` (`src/scenes/main_menu.c`) — das ROM lädt die Ersatztabelle
  vor dem Aufruf nach R4; erzwingt man das mit einer lokalen Variablen, kostet
  es ein zusätzliches Callee-saved-Register.

`scene_set_sprite_motion_sine_vel` im Detail: Der Quelltext ist
rekonstruiert und semantisch korrekt, aber die Registerzuteilung weicht ab: das
ROM parkt das letzte Argument in `IP` und gibt `R5` für das häufiger benutzte
frei, agbcc entscheidet umgekehrt. Sieben gleichzeitig lebende Kurzwerte sind
genau die Grenze, an der diese Entscheidung kippt.

### Eingepflegt

`src/lib_sprite.c` ist von 28 auf **26** verbleibende asm-Includes gesunken;
`sprite_anim_get_cel_total` und `sprite_get_anim_duration` stehen als C an ihrer
ursprünglichen Position. Die beiden Rückgabetypen in `src/lib_sprite.h` sind auf
`u8` bzw. `u16` korrigiert. Vorher wie nachher gilt: die gesamte Einheit ist
5332 Byte groß und reproduziert das ROM byte-genau.

Die beiden nun ungenutzten Dateien `asm/lib_sprite/asm_080eed9c.s` und
`asm_080eedc0.s` sind absichtlich liegengelassen — sie werden nicht mehr
eingebunden und stören den Bau nicht.

## PC-Build

Zwei ausführbare Dateien, beide über `make -C pc`:

| Ziel | Was es tut |
|---|---|
| `pc/wariowarepc` | Der Betrachter. Lädt einen Szenen-Blob und die Animationsbank, fährt den nativen Sprite-Pipeline und zeigt das Ergebnis in einem SDL2-Fenster. Läuft. |
| `pc/wariowarepc-game` | Der dekompilierte Spielcode selbst, so weit er kommt. Bricht an der ersten noch nicht portierten Funktion ab und nennt sie. |

```
make -C pc
./pc/wariowarepc                    # Fenster, 3x skaliert
./pc/wariowarepc --frames 300       # nicht-interaktiver Selbsttest
```

Tasten: `X`/`Z` = A/B, Enter = Start, Rechte Umschalt = Select, `A`/`S` = L/R,
Pfeiltasten = Steuerkreuz, Leertaste friert ein Bild ein, Esc beendet.

### Zwei Compiler, mit Absicht

`pc/Makefile` benutzt **gcc-15** für den Spielcode und **clang** für alles, was
das macOS-SDK anfasst. Beides ist notwendig:

* Der dekompilierte Code lebt von GNU-C89-Semantik. Eine implizite Deklaration,
  die später mit der echten Definition kollidiert, ist dort eine Warnung — unter
  clang ein harter Fehler. Prototypen nachzureichen ist keine Option: agbcc'
  Argumentkonvertierungen hängen genau daran (Regel 4).
* Homebrews GCC bringt eine reparierte Kopie von Apples `<stdio.h>` mit, die
  `<_bounds.h>` einbindet — ein Header, den das installierte SDK nicht hat. Der
  Shell, der Compositor und die Lader werden deshalb mit clang gebaut.

`include/global.h` bindet `<stdio.h>` nur für `NULL` ein; `pc/compat/stdio.h`
schattet es für den Spielcode ab, damit die Quellen unangetastet bleiben.

`-fcommon` ist nötig, weil die Spiel-Header Objekte wie `gGraphicsBuffer` als
tentative Definitionen führen. Auf dem GBA verschmolzen die; GCC 10 hat
`-fno-common` zum Standard gemacht.

### Zeigergrößen

Der GBA ist 32 Bit, und die Strukturen der Dekompilierung setzen 4-Byte-Zeiger
voraus. `-m32` wäre also richtig — **auf diesem Mac aber unmöglich**: macOS hat
32-Bit-Unterstützung vollständig entfernt, und auf Apple Silicon gibt es
überhaupt keinen 32-Bit-Userland. `-m32` bildet hier auf `-arch armv4t` ab und
scheitert im Linker.

Die Sorge dahinter ist berechtigt, trifft aber erst später zu. Solange die
Strukturen nur RAM beschreiben, den der C-Code selbst anlegt, ist das Layout in
sich stimmig — `sizeof` ändert sich, aber alle Zugriffe ändern sich mit. Kritisch
wird es genau dann, wenn ROM-Datentabellen dazukommen: die enthalten
32-Bit-Zeiger im Abbild. Der saubere Ausweg ist, diese Tabellen aus `data/*.s`
nach C zu überführen, sodass der Host-Linker Zeiger in Host-Breite einsetzt.
Das ist mechanische Arbeit und steht an, bevor echte Spieldaten geladen werden.

### Die Hardware-Naht

Der Port muss überraschend wenig ersetzen. Das Spiel schreibt nie direkt in die
Hardware: es pflegt einen Schatten des Anzeigezustands in IWRAM, und *eine*
Funktion schiebt ihn einmal pro Bild mit drei DMA-Bursts zur PPU. Alles andere
läuft über vier schmale Türen.

| Was | Wo im ROM | Ersatz |
|---|---|---|
| Bildausgabe | `flush_graphics_buffer` | `pc/gba_runtime.c` → Compositor |
| Tasten | `REG_KEY` in `update_key_buffers` | `pc/gba_hw.c` ← SDL-Events |
| Blocktransfers | `dma3_set` / `dma3_fill` | `pc/gba_hw.c`, schlichte Kopien |
| Bildtakt | VBlank-IRQ | `gba_hw_frame_wait`, 59.7275 Hz |
| Ton | Sappy-Treiber, noch Assembly | offen |

Gemessen: der Compositor kostet 0.31 ms pro Bild, der komplette Frame 1.7 ms von
16.74 ms Budget. Die Bildrate liegt bei 59.73 Hz gegen 59.7275 Hz Ziel — die
Frist wird absolut geführt, nicht relativ, sonst summiert sich der Schlaffehler
von `SDL_Delay` auf etwa 56 Hz herunter.

## Der Spielcode startet

`pc/wariowarepc-game` führt die Initialisierung aus, dreht die Hauptschleife und
startet Szenen:

```
./pc/wariowarepc-game --frames 60 --scene 1
```

```
Rufe func_080001D4 (Spiel-Initialisierung) auf ...
Initialisierung durchgelaufen.
Starte bei Szene 1.

[stub] func_0800A7D4 ist noch nicht portiert -- das Spiel kommt bis hierher.
Aufrufpfad:
1  func_0800A7D4
2  update_active_beatscript_scene
3  func_08016D00
4  title_scene_run
5  main
```

`--scene N` überspringt die Init-Szene. Sie übergibt immer ans Intro, und dessen
Daten liegen noch als roher ROM-Block vor; Szene 1 ist der Titelbildschirm, die
einzige Szene, deren Daten heute vollständig als C vorliegen.

### Jeder Fehlschlag nennt seinen Aufrufpfad

Drei Meldewege, alle über `backtrace_symbols_fd` und `write(2)`, weil die
Spiel-Header `<stdio.h>` verdecken:

| Meldung | Ursache |
|---------|---------|
| `[stub] NAME ist noch nicht portiert` | Funktion fehlt noch |
| `[fault] Speicherzugriffsfehler` + Adresse + Befehlszähler | meist ein genullter Datenblock |
| `[hal] dma3_set: N Bytes abgelehnt` | Operanden aus Stubs, Größe unplausibel |

Ohne den Aufrufpfad ist ein genullter Datenblock nicht auffindbar: das Programm
stirbt mit Signal 11 und sagt nichts. Der Fault-Melder gibt zusätzlich die
Zugriffsadresse und den Befehlszähler aus — `backtrace()` läuft über
Frame-Pointer und lässt Blattfunktionen weg, der gesicherte Kontext hat die
fehlerhafte Instruktion exakt. Deshalb baut der PC-Build mit
`-fno-omit-frame-pointer` und `-MMD -MP`: ohne Header-Abhängigkeiten bleiben
nach einer Struktur-Änderung veraltete Objekte liegen, und der Absturz sieht
dann wie ein Code-Fehler aus.

### Wie eine Funktion in den Port kommt

Der Zyklus ist immer derselbe: laufen lassen, den gemeldeten Namen bearbeiten,
wieder laufen lassen. Für die Entscheidung gibt es genau zwei Fälle.

**Portierbar** heißt: dekompilieren, byte-genau prüfen, fertig. Der PC-Build
nimmt dieselbe C-Datei.

**Nicht portierbar** heißt: die Funktion bleibt Assembly, und die Laufzeit
stellt eine native Entsprechung bereit. Das geht, weil eine noch nicht
dekompilierte Funktion im PC-Build schlicht fehlt — `-Dasm(x)=` lässt die
eingebundene Assembly verschwinden — und der Platz damit frei ist. Eintrag in
`pc/provided.txt`, Implementierung in `pc/gba_runtime.c`.

Bisher trifft der zweite Fall drei Muster:

* **Selbstverschiebender Code.** `func_080EE644` kopiert eine kleine Routine auf
  den Stack und springt hinein, weil SRAM nicht aus dem Cartridge heraus gelesen
  werden darf. Im Port ist es eine Byte-Kopie.
* **ARM-Blobs.** `func_08002620`, `func_0800774C` und `func_08003DA0` kopieren
  handgeschriebene ARM-Routinen (Division, Wurzel, RLE-Dekompression) nach IWRAM
  und rufen sie über Zeiger auf. ARM-Maschinencode läuft hier nicht.
* **Der Soundtreiber.** Vier Einstiegspunkte, vorerst stille No-ops.

`src/code_08000f10.c` ist der Sonderfall: es *ist* die Hardwareschicht, die
einzige Einheit, die `REG_`-Register anfasst. Der PC-Build linkt sie nicht,
sondern `pc/gba_hal.c` — dieselbe Rechenlogik, andere vier Türen.

### Neue Übersetzungseinheiten

Die Disassembly lässt jede noch nicht zugeordnete Funktion als eigene Datei
unter `asm/` liegen, und das Linker-Skript nennt sie einzeln in ROM-Reihenfolge.
`tools/new_unit.py` schneidet daraus eine Einheit: es verschiebt einen
zusammenhängenden Lauf in ein Unterverzeichnis — wo der `asm/*.s`-Glob des
Builds ihn nicht mehr sieht —, wandelt die Dateien in die `asm("...")`-Form um,
schreibt die `.c` und fasst die Linker-Zeilen zu einem Objekt zusammen.

```
python3 tools/new_unit.py 0x0800C934 0x0800DB04 code_0800c934
```

Die Grenzen sind nicht geraten: eine Einheit endet dort, wo die nächste bereits
bekannte beginnt. `code_08002620` reicht genau bis zum Anfang von
`memory_heap.c`.

### Struktur-Offsets

Der Host-Compiler ist hier keine Hilfe: seine Zeiger sind acht Byte, also wäre
jeder Offset hinter dem ersten Zeiger falsch. `tools/offsets.py` fragt
stattdessen agbcc, indem es eine Sonde übersetzt, deren Initialisierer die
Offsets sind, und sie aus der erzeugten Assembly zurückliest.

```
python3 tools/offsets.py types.h BeatscriptScene sizeof threads unk168
```

```
  sizeof     0x1C90
  threads    0x0028
  unk168     0x1C30
```

### Zeigergrößen: der eigentliche Gegner auf 64 Bit

Der Hinweis, mit `-m32` zu bauen, trifft den Kern. Auf Apple Silicon gibt es
kein 32-Bit-Userland, also muss der Port die Stellen einzeln behandeln, an denen
der GBA-Code Adressen in 32-Bit-Wörtern ablegt. Drei Muster, alle mit demselben
Symptom: eine Adresse verliert ihre oberen 32 Bit.

**1. Zeiger in `u32`-Feldern.** `struct Beatscript` speichert Funktions- und
Datenadressen in `param2`/`param3`, `struct TitleSceneData` in `unk0`/`unk4`.
Dafür gibt es in `include/global.h`:

```c
#ifdef WWPC_WIDE_POINTERS
typedef unsigned long u32p;
#else
typedef u32 u32p;
#endif
```

agbcc sieht das Makro nie, `u32p` ist dort schlicht `u32` — die ROM-Ausgabe
ändert sich nicht, was `tools/verify_all.sh` nach jeder solchen Änderung
bestätigt.

**2. `(u32)`-Casts auf Zeiger.** `args[0] = (u32)subScene;` schneidet ab, auch
wenn `args` bereits `u32p` ist. Im Disassembler sieht man es sofort:
`and x19, x19, #0xffffffff`. Die Hardware-Register-Schreibzugriffe
(`REG_DMA3SAD = (u32)source`) sind dagegen echt 32 Bit und bleiben.

**3. Implizite Deklarationen.** Die tückischste Variante: unter C89 gilt eine
nicht deklarierte Funktion als `int`-liefernd, was jeden zurückgegebenen Zeiger
abschneidet. Von 166 implizit deklarierten Funktionen liefern genau vier einen
Zeiger — `func_0800A228`, `func_0800C874`, `mem_heap_alloc_id`, `play_sound`.
Alle vier haben inzwischen eine Deklaration an ihren Aufrufstellen. Die Prüfung
ist mechanisch:

```
gcc-15 -fsyntax-only -Wimplicit-function-declaration ... src/*.c src/scenes/*.c
```

### Der Dispatcher (`func_0800A7D4`)

Der Opcode-Dispatcher des Beatscripts ist der letzte Block vor einem laufenden
Titelbildschirm. Gemessen statt geschaetzt:

| | |
|---|---|
| Instruktionen | 1574 |
| Sprungtabelleneintraege | 157 (Opcode 0x00..0x9C) |
| verschiedene Fallkoerper | 106 (105 echte plus der Epilog) |
| Median je Fall | 24 Byte |
| aufgerufene Funktionen | 63, davon 46 schon als C |

Ein Entwurf liegt unter `wip/func_0800A7D4.c` — 705 Zeilen, **noch nicht
byte-genau** und bewusst nicht im Build. Auf Instruktionsebene verglichen
(Mnemonics ohne Register) bleiben 20 Abweichungsstellen bei 1350 Instruktionen,
und die Gesamtzahl stimmt auf drei genau; die Struktur steht also.

Zwei Dinge fehlen. Erstens der Prolog: das ROM legt `threadIndex` auf den Stack
und haelt `isSceneMode` in SL, der Entwurf macht es umgekehrt. Das ROM haelt
sechs langlebige Werte im Register und laesst R5 als Kratzregister frei — der
Entwurf haelt sieben. Irgendwo fehlt also ein Zwischenwert, der Druck erzeugt.
Weil der Rahmen dadurch 4 Byte kleiner ist, verschiebt sich alles danach, und
der Byte-Vergleich taugt bis dahin nicht als Fortschrittsmass. Zweitens laufen
im ROM mehrere Verzweigungen in gemeinsame Endstuecke zusammen (Cross-Jumping),
was hier noch nicht passiert.

`wip/func_0800A7D4_faelle.txt` enthaelt die Disassembly nach Faellen sortiert,
mit den Opcode-Namen aus `include/beatscript_main.inc` — das ist die
Arbeitsgrundlage.

**Messmethode.** Bei einer Funktion dieser Groesse ist der Byte-Vergleich
nutzlos, solange der Prolog abweicht: eine einzige Registerentscheidung
verschiebt alles. Der Vergleich der Instruktionsfolge ohne Register trennt
"Struktur falsch" von "Register anders" und zeigt, welcher Fall wirklich noch
klemmt.

## Die Daten sind der zweite Berg

Ab einem gewissen Punkt ist der Boot-Pfad nicht mehr code-, sondern
datengebunden. Der GBA-Build nimmt Daten so, wie sie im ROM liegen: Beatscripts
als assemblierte `.bs`, alles übrige als rohe Blöcke unter `bin/` (3,8 MB in 14
Dateien). Beide Formen speichern Zeiger als vier Byte.

Erzeugte Ersatzdaten liegen unter **`pcdata/`**, bewusst außerhalb des
Decomp-Baums: dessen Makefile sammelt `data/*.c` ein, und ein zweites
`scene_title` würde den ROM-Build zerlegen.

### `tools/bs2c.py` — Beatscript nach C

Statt die 51 Makros aus `include/beatscript_main.inc` nachzubauen, lässt das
Werkzeug den echten Assembler arbeiten und liest das Ergebnis zurück: die
Sektionsbytes liefern die Literale, die Relocations die Symbole, die globalen
Labels die Array-Grenzen. Die `.bs`-Quelle wird nur danach befragt, ob ein Label
ein `script`, ein `struct` oder ein `text` ist.

```
python3 tools/bs2c.py data/scenes/title/scene.bs --verify
```

```
4 Labels, 20 Symbole, 1116 Bytes -> pcdata/scenes/title/scene_bs.c
Rueckuebersetzung: 1116 Bytes, 0 Abweichungen (0 an Relocation-Stellen)
IDENTISCH -- die Uebersetzung gibt die .bs-Daten exakt wieder
```

`--verify` ist der Kern: es übersetzt das erzeugte C mit **agbcc** zurück, wo
`u32p` wieder `u32` ist, und vergleicht Byte für Byte mit der assemblierten
`.bs`. Eine treue Übersetzung *muss* deckungsgleich sein. Nur was diese Probe
besteht, geht in den Build.

Referenzierte Symbole bekommen einen expliziten Assembler-Namen:

```c
extern char bs_ref_title_bg_pal[] __asm__("_title_bg_pal");
```

Damit muss die erzeugte Datei sich mit keinem Header über Typen einigen.

### `tools/rom2c.py` — Rohblock nach C

Für Daten, die noch niemand rekonstruiert hat. Das Werkzeug **erfindet keine
Struktur**: es gibt den ROM-Block Byte für Byte als C-Array aus und drückt nur
die benannten Zeigertabellen als echte C-Zeiger *in dieses Array* aus.

```
python3 tools/rom2c.py 0x083A4A80 0x083A8CAC -o pcdata/rom_083A4A80.c \
    --include src/scenes/gameplay.h \
    --table 0x083A4B58 29 D_083A4B58 'struct Unk083A4B58 *'
```

Das hat den Boot freigemacht: `init_save_buffer` legt acht Bestwerte an und
greift dafür über `D_083A4B58[stage]` auf die Stage-Beschreibung zu. Als
genullter Stub war das ein Zugriff auf Adresse `0x2`.

## Renderer

`renderer/` enthält einen Software-Compositor für die GBA-Tile-Pipeline, ohne
externe Abhängigkeit im Kern (nur zlib für die PNG-Ausgabe). Er verarbeitet
denselben Zustand, den `flush_graphics_buffer` einmal pro Bild an die Hardware
gibt — Anzeigeregister, Palettenschatten, VRAM und OAM —, damit der Port den
Hardware-Schreibvorgang gegen einen Aufruf hierher tauschen kann, ohne davor
etwas zu ändern.

Umfang: Modus 0 (vier Text-Hintergründe) plus reguläre Sprites. Affine
Hintergründe und Fenster fehlen.

| Programm | Zweck |
|---|---|
| `renderer/render_scene` | lädt einen Szenen-Blob, rendert ein Bild, schreibt PNG — der kopflose Prüfpfad |
| `renderer/render_sprites` | treibt zusätzlich die native Sprite-Pipeline und schreibt Bildfolgen |

## Sprite-Pipeline (nativ)

`renderer/sprite_engine.c` bildet `lib_sprite` nach. Das Verhalten folgt den
dekompilierten und byte-geprüften Originalen; die Strukturen sind aber für einen
64-Bit-Host ausgelegt statt den 56-Byte-Datensatz des GBA zu kopieren — Zeiger
sind echte Zeiger, Ids bleiben 16-bittig vorzeichenbehaftet, wie das Spiel sie
benutzt.

Drei Stufen, wie im Original:

```
sprite_engine_emit()   laeuft einmal pro Bild ueber die z-sortierte Liste
expand_cel()           macht aus einer Animationszelle OAM-Eintraege
der Aufrufer            reicht den OAM-Puffer an gba_ppu_render()
```

Das ist bewusst *keine* byte-genaue Portierung: es ist die Anzeigeseite, nicht
die Spiellogik. Sobald `src/lib_sprite.c` vollständig dekompiliert ist, tritt es
an ihre Stelle.

## Reihenfolge des Aufrufs

`AgbMain` selbst ist noch Assembly — sein Vorspann ist ein Hardware-Reset, mit
dem ein PC-Build nichts anfangen kann. Die Schleife steht deshalb in
`pc/game_main.c` nachgebildet:

```c
vblank_wait();
get_random_u16();
func_080F3C78();          /* Soundtreiber-Tick */
update_key_buffers();
D_03003840++;

{
    short previous = gCurrentScene;

    run_current_scene();

    if (D_03003638 != 0 && (gCurrentKeys & 0xF) == 0xF) {
        gCurrentScene = 6;    /* A+B+Select+Start setzt zurueck */
    }
    D_030035E0 = (gCurrentScene == previous) ? 0 : 1;
}

func_080F3A90();          /* Soundtreiber-Bildende */
```

`D_030035E0` ist der Schalter, an dem jede Szene erkennt, ob sie gerade
angefangen hat: ist er ungleich null, läuft die Startfunktion, sonst die
Bildfunktion.

## Verifikation

* Vier extrahierte Assets sind **byte-identisch** mit den Referenzdateien
  im Decomp-Repo: `title_bg_tiles.4bpp` (7778 B), `title_bg_map_logo.tilemap`
  (1458 B), `title_bg_map_border.tilemap` (1788 B), `title_bg_map.tilemap`
  (1722 B). Das ist der Beweis, dass der Decoder korrekt ist.
* Die rekonstruierte Tabelle @0x083A8CAC deckt sich Eintrag für Eintrag und in
  der Reihenfolge mit `../wariowareinc-main/data/scenes/title/data.c`.
* Stichprobe über 120 Assets: Nibble-Entropie im Median 2,30 (Zufallsdaten
  lägen bei 4,00), Anteil Farbindex 0 im Median 39 % (Zufall ~6 %).
* Alle 23 Übersetzungseinheiten reproduzieren das ROM byte-genau; `bs2c.py`
  beweist seine Übersetzung durch Rückübersetzung.

## Wichtige Fallstricke

* **Paletten-Indizes zur Laufzeit behalten, nicht zu RGB einbacken.** Das
  Fade-System mutiert die 512-Farben-Palette jeden Frame. Wer Kacheln zu RGBA
  flachklopft, verliert sämtliche Überblendungen.
* **Bewusste Über-Lesungen.** Einträge der Grafiktabelle kopieren 0x200 Byte
  aus Arrays, die als 96 Byte deklariert sind. Der GBA las benachbartes ROM.
* **Uninitialisierte Reste.** `title_bg_tiles` dekomprimiert 7778 Byte in ein
  8192-Byte-Fenster; 414 Byte bleiben alter VRAM-Inhalt.
* **Header-Änderungen prüfen die ganze Menge.** Ein neuer Prototyp kann die
  Argumentumwandlung an *jeder* Aufrufstelle ändern. Nach jeder Änderung an
  einem gemeinsam genutzten Header `sh tools/verify_all.sh`.

## Referenz-Deckung

Von 1050 Assets sind **913 (87 %)** referenziert: 825 über Grafiktabellen,
der Rest über `buffered_textures`-Listen.

Die 11 `FUNCTION_GFX_SOURCE`-Einträge zeigen alle auf denselben Loader
`func_08012D7C` — eine 47-zeilige Thumb-Funktion, die noch als Assembly in
`src/scenes/main_menu.c` eingebunden ist. Was sie lädt, ist erst nach ihrer
Dekompilierung bekannt.

Die verbleibenden 137 Assets (0x08548ED4..0x087E017C) sind über keinen der drei
Wege erreichbar — vermutlich Tabellen, deren Adresse zur Laufzeit berechnet wird.

## Stand

| | |
|---|---|
| Übersetzungseinheiten | 23, **alle byte-genau gegen das ROM** |
| offene Assembly-Includes darin | 429 |
| noch nicht zugeordnete Funktionen | ~5470 lose `.s`-Dateien |
| fehlende Symbole im PC-Build | 250 (116 Funktionen, 134 Datenblöcke) |
| ROM-Daten in C | zwei Beatscripts, ein Rohblock, Titelgrafik |
| ROM-Daten roh | 3,8 MB in 14 `bin/`-Blöcken |

Nach jeder Änderung an einem gemeinsam genutzten Header:

```
sh tools/verify_all.sh
```

### Wie weit die Titelszene kommt

```
main -> title_scene_run -> func_08016CBC -> set_beatscript_subscenes
                                         -> title_scene_start   (durch)
     -> func_08016D00 -> task_pool_update_delayed   (durch)
                      -> task_pool_update_constant  (durch)
                      -> update_active_beatscript_scene
                         -> func_0800A7D4           <- hier
```

`func_0800A7D4` ist der Opcode-Dispatcher des Beatscripts: 1574 Instruktionen,
eine Sprungtabelle über rund 0x9C Befehle. Das ist der nächste große Brocken und
der einzige, der noch zwischen dem Port und einem laufenden Titelbildschirm
steht.

## Noch nicht extrahiert

* ~12.631 Animations-Datensätze in `bin/data_0830C3B0.bin`
* 765 Instrument-Datensätze in `bin/data_083AB638.bin`
* Die Paletten des Spiels (nur der Titelbildschirm liegt als C-Quelle vor)
* Die Intro-Szene: `D_083AB754` ist eine `SubScene` im Rohblock, ohne
  `.bs`-Quelle. Ihr Beatscript zeigt auf Funktionen und Grafiktabellen quer
  durchs ROM — ein Beatscript lässt sich erst übersetzen, wenn alles existiert,
  worauf es zeigt. Deshalb funktioniert der Titelbildschirm: seine `.bs` wurde
  von Hand mit echten Symbolnamen geschrieben.
