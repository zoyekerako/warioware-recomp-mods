#!/bin/sh
# Baut den WarioWare-Runner gegen gbarecomp. Von port/gbarecomp/ aus aufrufen.
#
# Voraussetzungen: cmake, SDL2 (brew), gbarecomp-Quellen unter ../../gbarecomp,
# rom.gba und gba_bios.bin im Projektwurzelverzeichnis.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$HERE/../..
SRC=$ROOT/gbarecomp/gbarecomp-0.2.0
BUILD=${BUILD:-$HERE/build}
GEN=$BUILD/generated

mkdir -p "$BUILD" "$GEN"

# 1. Kern bauen (Recompiler + Laufzeitbibliotheken)
cmake -S "$SRC" -B "$BUILD/cmake" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$BUILD/cmake" --target gba_recompile -j8 >/dev/null

# 2. BIOS rekompilieren, damit SWI-Aufrufe echten BIOS-Code ausfuehren
cp "$ROOT/gba_bios.bin" "$SRC/bios/" 2>/dev/null || true
"$BUILD/cmake/gba_recompile" --bios "$SRC/bios/gba_bios.bin" \
    --config "$SRC/bios/gba_bios.toml" >/dev/null

# 3. Laufzeit bauen (jetzt mit BIOS-Ausgabe)
cmake -S "$SRC" -B "$BUILD/cmake" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$BUILD/cmake" --target gbarecomp_runtime -j8 >/dev/null

# 4. Die ROM rekompilieren
"$BUILD/cmake/gba_recompile" --rom "$ROOT/rom.gba" \
    --config "$HERE/wariowareinc.toml" --out "$GEN"

# 5. Runner linken -- zusammen mit den Mods aus mod/ (Menue + Zufallsmodus).
#    Die Naht dafuer sitzt in runtime.cpp und ist als mod-seam.patch abgelegt.
C=$BUILD/cmake
c++ -std=c++17 -O1 -o "$HERE/wariowarerecomp" \
    "$HERE/runner/main.cpp" "$HERE/mod/ww_mod.cpp" \
    "$GEN"/recompiled_*.cpp "$GEN/dispatch_table.cpp" "$GEN/symbol_map.cpp" \
    "$SRC/src/runtime/generated_bios/bios_recompiled.cpp" \
    "$SRC/src/runtime/generated_bios/bios_dispatch_table.cpp" \
    -I "$SRC/src/runtime" -I "$SRC/src/runtime/generated_bios" \
    -I "$SRC/src/armv4t" -I "$SRC/src/gba" -I "$SRC/src/debug" \
    -I "$SRC/src/recompile" -I "$GEN" -I "$HERE/mod" \
    -I /opt/homebrew/include/SDL2 -DGBARECOMP_HAVE_SDL2 \
    "$C/libgbarecomp_runtime.a" "$C/libgbarecomp_gba.a" \
    "$C/libgbarecomp_recompile_core.a" "$C/libgbarecomp_armv4t.a" \
    "$C/libgbarecomp_debug.a" "$C/libgbarecomp_heal_gate.a" \
    -L/opt/homebrew/lib -lSDL2

echo "fertig: $HERE/wariowarerecomp"
