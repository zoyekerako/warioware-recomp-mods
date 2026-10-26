# WarioWare Inc. — PC-Port und Mods

Zwei Wege zum selben Spiel auf dem Rechner, und die Arbeit, die dabei
angefallen ist:

* **`port/gbarecomp/`** — die spielbare Fassung. `gbarecomp` übersetzt die ROM
  statisch nach C++; darüber liegt ein Mod-Layer, das kein Byte der ROM
  anfasst: Mod-Menü, Zufallsmodus, KaizoWare, Boss Rush, Lautstärkeregler,
  freie Fenstergrößen samt Breitbild und Hochkant, Zeigersteuerung für Maus
  und Finger — und Klicks in den Menüs des Spiels selbst.
  → [`port/gbarecomp/README.md`](port/gbarecomp/README.md)
* **`port/`** — die Werkzeuge der Portierung: Prüfer gegen die ROM,
  Struktur-Rekonstruktion, Grafik- und Beatscript-Pipeline, PC-Quelltext.
  → [`port/README.md`](port/README.md)
* **`wariowareinc-main/`** — die Dekompilierung, gegen die alles geprüft wird.
  Jede Übersetzungseinheit muss byte-genau dieselbe Maschinensprache ergeben
  wie das Original; `sh port/tools/verify_all.sh` sagt, ob das noch stimmt.

## Was hier nicht liegt

**ROM und BIOS fehlen, und das mit Absicht.** `rom.gba`, `gba_bios.bin`,
`rom.sav` und `wariowareinc-main/baserom.gba` musst du selbst beistellen — aus
deinem eigenen Modul und deinem eigenen Gerät. Ebenso fehlt das übersetzte
BIOS unter `src/runtime/generated_bios/`: `build.sh` erzeugt es in Sekunden
aus deinem Abzug neu.

Auch der `gbarecomp`-Klon selbst liegt nicht hier — das ist fremder Quelltext
mit eigener Lizenz. Die drei Änderungen daran stehen als Patches daneben:
`mod-seam.patch` (die Naht für das Mod-Layer, sieben Aufrufe gegen schwach
definierte No-Ops), `audio-rate-fix.patch` und `sound-dma-irq-fix.patch`.

## Bauen

```sh
sh port/gbarecomp/build.sh        # braucht cmake und SDL2 aus brew
./port/gbarecomp/wariowarerecomp \
    --config port/gbarecomp/wariowareinc.toml \
    --rom rom.gba --bios gba_bios.bin
```

**F10** öffnet das Mod-Menü. Alles Weitere steht in
[`port/gbarecomp/README.md`](port/gbarecomp/README.md) — samt der Messungen,
mit denen die Befunde belegt sind.

## Herkunft

Die Dekompilierung geht auf das öffentliche `wariowareinc`-Projekt zurück,
die spielbare Fassung auf `gbarecomp`. WarioWare, Inc.: Mega Microgame$ ist
© 2003 Nintendo; von Nintendo stammt hier nichts, was nicht aus einer
eigenen ROM gewonnen werden muss.
