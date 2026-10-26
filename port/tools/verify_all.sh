#!/bin/sh
# Verify every translation unit against the ROM.  Run from port/.
# Two units carry no func_XXXXXXXX symbol to derive their base from, so those
# bases are spelled out here.
cd "$(dirname "$0")/.." || exit 1
fail=0
for u in $(ls ../wariowareinc-main/src/*.c ../wariowareinc-main/src/scenes/*.c | sed 's|../wariowareinc-main/||'); do
    case "$u" in
        src/palette.c)      base=0x080011C4 ;;
        src/scenes/title.c) base=0x0800DC08 ;;
        *)                  base= ;;
    esac
    line=$(python3 tools/verify_file.py "$u" $base 2>&1 | grep '^mismatches')
    printf "%-32s %s\n" "$u" "$(echo "$line" | sed 's/^mismatches *: *//')"
    echo "$line" | grep -q '^mismatches  : 0 real' || fail=1
done
[ $fail -eq 0 ] && echo "\nalle Einheiten byte-genau" || echo "\nFEHLER: mindestens eine Einheit weicht ab"
exit $fail
