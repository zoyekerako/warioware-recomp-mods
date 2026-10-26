# pcdata -- ROM-Daten in C-Form, nur fuer den PC-Build

Der GBA-Build nimmt die Daten so, wie sie im ROM liegen: Beatscripts als
assemblierte `.bs`, alles uebrige als rohe Bloecke unter `bin/`. Beide Formen
speichern Zeiger als vier Byte. Auf einem 64-Bit-Host ist das unbrauchbar --
die Adressen wuerden abgeschnitten -- also muss dieselbe Information vom
Host-Compiler ausgelegt werden.

Die Dateien hier sind erzeugt, nicht von Hand geschrieben, und sie liegen
bewusst **ausserhalb** des Decomp-Baums: dessen Makefile sammelt `data/*.c`
ein, und ein zweites `scene_title` wuerde den ROM-Build zerlegen.

| erzeugt von            | Inhalt                                    |
|------------------------|-------------------------------------------|
| `tools/bs2c.py`        | `scenes/*/scene_bs.c` -- Beatscripts       |

`bs2c.py --verify` uebersetzt sein Ergebnis mit agbcc zurueck und vergleicht
es Byte fuer Byte mit der assemblierten `.bs`. Nur was dort deckungsgleich
ist, kommt hier an.
