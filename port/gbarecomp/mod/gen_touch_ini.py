# -*- coding: utf-8 -*-
"""Erzeugt touch.ini aus den Steuerungshinweisen der ROM.
Aufruf:  python3 gen_touch_ini.py <rom.gba> <ziel.ini> [erste] [letzte]

Jede Microgame-Beschreibung traegt am Ende ihres Erklaertextes die Symbole,
die das Spiel selbst fuer die Steuerung einblendet, als Escape der Form

    .e:1QR:0.3JUMP      .e:1GH:0.3MOVE

QR ist der Knopf (JUMP, SHOOT, GRAB, CATCH, DROP, ENTER -- 132 Fundstellen),
GH/OP/IJ/EF/MN sind Richtungen (MOVE, STEER, AIM, DUCK, SELECT).
Daraus folgt das Schema unmittelbar:

    nur Knopf      -> tap     (Aufsetzen = A)
    nur Richtung   -> drag    (Ziehen = gehaltene Richtung)
    beides         -> both
    nichts genannt -> kein Eintrag, es gilt die Menueeinstellung

Was daraus NICHT folgt, ist flick: ob ein Microgame gPressedKeys (Flanke)
oder gCurrentKeys (Pegel) liest, steht in seinem Code und nicht im Text.
Diese Faelle bleiben dem Spielen ueberlassen.
"""
import re, struct, sys, unicodedata

MG_TABLE   = 0x083A50E0   # D_083A50E0, 16 Byte je Eintrag
DESC_TABLE = 0x083A6090   # Beschreibungen, 4 Byte je Zeiger
DESC_SHIFT = 2            # Beschreibung[i + 2] gehoert zu Microgame[i]
# QR ist mit 132 Fundstellen eindeutig der Knopf. KL und OJ kommen je einmal
# vor, mit ATTACK ("kicks and punches") und GUARD -- nach dem Verb ebenfalls
# Knopf, nicht Richtung. CD kommt einmal ohne Verb vor und bleibt ungedeutet.
BUTTON = {'QR', 'KL', 'OJ'}
UNKNOWN = {'CD'}
WRAPPER_REST = 6   # D_083A50E0[i].unk8 -- Zwischenbild, keine Eingabe

# Von Hand gesetzt, wo der Erklaertext mehr sagt als das Symbol. Steht hier
# statt in der erzeugten Datei, damit ein erneuter Lauf es nicht wegwirft.
OVERRIDE = {
    60: ('hold', 'Fernlenk-Staubsauger: der Knopf lenkt, solange er gehalten wird'),
}
ESCAPE = re.compile(r'\.e:1([A-Z]{2}):0\.3\s*([^\.]*)')


def main(rom_path, out_path, first=0, last=49):
    rom = open(rom_path, 'rb').read()

    def u32(a):
        o = a - 0x08000000
        return struct.unpack('<I', rom[o:o + 4])[0]

    def text(a, n=240, enc='shift_jis'):
        o = a - 0x08000000
        b = rom[o:o + n]
        z = b.find(b'\x00')
        b = b[:z if z >= 0 else n]
        try:
            return b.decode(enc)
        except Exception:
            return b.decode('latin1', 'replace')

    # Die Beschreibungstabelle ist kuerzer als die Microgame-Tabelle. Ihr Ende
    # muss gefunden werden, sonst liest der Erzeuger hinter ihr Muell und
    # schreibt ihn als Begruendung in die Datei.
    desc_count = 0
    while 0x08000000 <= u32(DESC_TABLE + desc_count * 4) < 0x08800000:
        desc_count += 1

    rows = []
    for i in range(first, last + 1):
        wrapper = rom[MG_TABLE - 0x08000000 + i * 16 + 8]
        if wrapper == WRAPPER_REST:
            rows.append((i, 'off', 'Zwischenbild', ['keine Eingabe']))
            continue
        if i + DESC_SHIFT >= desc_count:
            rows.append((i, None, '(keine Beschreibung)', []))
            continue
        raw = unicodedata.normalize('NFKC', text(u32(DESC_TABLE + (i + DESC_SHIFT) * 4)))
        raw = raw.replace('_', ' ')
        title = raw.split('@0')[0].strip('【】 ') or '?'
        hits = ESCAPE.findall(raw)
        codes = {c for c, _ in hits} - UNKNOWN
        verbs = [v.strip() for _, v in hits if v.strip()]
        if not codes:
            rows.append((i, None, title, verbs))
            continue
        has_btn = bool(codes & BUTTON)
        has_dir = bool(codes - BUTTON)
        scheme = 'both' if (has_btn and has_dir) else ('tap' if has_btn else 'drag')
        if i in OVERRIDE:
            scheme, why = OVERRIDE[i]
            verbs = verbs + [why]
        rows.append((i, scheme, title, verbs))

    out = [__doc__.split('"""')[0]]  # leer, der Kopf kommt gleich von Hand
    L = []
    L.append('# touch.ini -- Zeigerschema je Microgame.')
    L.append('#')
    L.append('# Neben die ausfuehrbare Datei legen, oder ueber WWMOD_TOUCH_TABLE=<Pfad>')
    L.append('# woanders hin. Eine Zeile je Microgame:')
    L.append('#')
    L.append('#     <Nummer> = off | tap | drag | both | flick')
    L.append('#')
    L.append('#     tap    Aufsetzen = A, kurzer Impuls.')
    L.append('#     hold   A gedrueckt, solange der Zeiger aufliegt.')
    L.append('#     drag   Ziehen = gehaltene Richtung. Fuer "Figur bewegen".')
    L.append('#     both   beides.')
    L.append('#     flick  Wischen = eine einzelne Flanke. Fuer Spiele, die')
    L.append('#            gPressedKeys lesen statt gCurrentKeys.')
    L.append('#     off    Zeiger aus, nur Tastatur.')
    L.append('#')
    L.append('# Die Nummer ist currentMicrogameID; mit "Microgame readout" im Mod-Menue')
    L.append('# steht sie waehrend des Spiels im Bild.')
    L.append('#')
    L.append('# ── Woher diese Vorschlaege kommen ───────────────────────────────────')
    L.append('#')
    L.append('# Nicht geraten. Jede Microgame-Beschreibung (Tabelle 0x083A6090, um 2')
    L.append('# gegen D_083A50E0 versetzt) traegt am Ende die Symbole, die das Spiel')
    L.append('# selbst fuer die Steuerung einblendet:')
    L.append('#')
    L.append('#     .e:1QR:0.3JUMP        QR = Knopf   (132 Fundstellen)')
    L.append('#     .e:1GH:0.3MOVE        GH = Richtung (76)')
    L.append('#                           OP (41), IJ (12), EF (4) ebenfalls Richtung')
    L.append('#                           KL und OJ je 1x, nach ihrem Verb (ATTACK,')
    L.append('#                           GUARD) als Knopf gewertet; CD 1x ohne Verb,')
    L.append('#                           ungedeutet und ohne Vorschlag')
    L.append('#')
    L.append('# nur Knopf -> tap, nur Richtung -> drag, beides -> both. Der Kommentar')
    L.append('# hinter jeder Zeile nennt Titel und Verben, damit der Grund sichtbar')
    L.append('# bleibt und ein Irrtum schnell zu finden ist.')
    L.append('#')
    L.append('# Was daraus NICHT folgt, ist flick: ob ein Microgame die Flanke oder den')
    L.append('# Pegel liest, steht in seinem Code, nicht im Text. Wo sich ein Spiel')
    L.append('# traege anfuehlt, ist "flick" der naechste Versuch.')
    L.append('#')
    L.append('# Zwischenbilder (D_083A50E0[i].unk8 == 6) bekommen "off": dort ist nichts')
    L.append('# zu bedienen. Die Beschreibungstabelle reicht nur bis Microgame 225; was')
    L.append('# danach kommt -- die letzten freischaltbaren Spiele -- bleibt ohne')
    L.append('# Vorschlag, weil es dafuer keinen Hinweis in den Daten gibt.')
    L.append('#')
    L.append('# Erzeugt von gen_touch_ini.py -- Aenderungen von Hand bleiben, solange')
    L.append('# das Skript nicht erneut darueberlaeuft.')
    L.append('')
    for i, scheme, title, verbs in rows:
        note = title if not verbs else f'{title} -- {", ".join(dict.fromkeys(verbs))}'
        if scheme is None:
            L.append(f'# {i:3d} = ?        # {note}   (kein Hinweis im Text)')
        else:
            L.append(f'{i:3d} = {scheme:<5}    # {note}')
    open(out_path, 'w').write('\n'.join(L) + '\n')
    named = sum(1 for r in rows if r[1])
    print(f'geschrieben: {out_path} -- {named} von {len(rows)} Microgames mit Vorschlag')


if __name__ == '__main__':
    a = sys.argv
    main(a[1], a[2], int(a[3]) if len(a) > 3 else 0, int(a[4]) if len(a) > 4 else 49)
