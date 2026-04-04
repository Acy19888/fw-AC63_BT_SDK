#!/usr/bin/env python3
"""
patch_tone_cfg.py - Ersetzt power_on.wtg in tone.cfg mit einer neuen .wtg Datei
Aufruf: python patch_tone_cfg.py <neue_power_on.wtg>
"""
import sys, struct, os, shutil

TONE_CFG = os.path.join(os.path.dirname(__file__), 'tone.cfg')
BACKUP   = TONE_CFG + '.backup'

def parse_entries(data):
    """Liest alle Directory-Eintraege aus tone.cfg"""
    entries = []
    off = 0x0010
    while off + 32 <= len(data):
        entry = data[off:off+32]
        name_field = entry[:16]
        null_pos   = name_field.find(b'\x00')
        if null_pos == 0:
            break
        name       = name_field[:null_pos].decode('latin-1')
        checksum   = struct.unpack_from('<I', entry, 16)[0]
        file_off   = struct.unpack_from('<I', entry, 20)[0]
        file_size  = struct.unpack_from('<I', entry, 24)[0]
        flags      = struct.unpack_from('<I', entry, 28)[0]
        entries.append({'name': name, 'checksum': checksum,
                        'offset': file_off, 'size': file_size,
                        'flags': flags, 'dir_off': off})
        off += 32
    return entries

def build_tone_cfg(data, entries, replacements):
    """
    Baut eine neue tone.cfg:
    - Kopiert Header + Directory-Bereich
    - Ersetzt File-Inhalte laut replacements {name: bytes}
    - Passt Offsets an (falls Groesse sich aendert)
    """
    # Finde den Bereich vor den eigentlichen Dateidaten
    # Die erste Datei (nach 'tone' und 'index.idx') bestimmt den Datenbeginn
    data_entries = [e for e in entries if e['name'] not in ('tone', 'index.idx')]
    meta_entries = [e for e in entries if e['name'] in ('tone', 'index.idx')]

    # Datenbeginn = kleinster Offset unter allen Eintraegen
    data_start = min(e['offset'] for e in entries if e['offset'] < 0x10000000)
    print(f"Datendaten beginnen bei: 0x{data_start:x}")

    # Basisstruktur kopieren (bis data_start)
    result = bytearray(data[:data_start])

    # Meta-Dateien unveraendert einbauen (tone, index.idx)
    for e in meta_entries:
        file_data = data[e['offset']:e['offset']+e['size']]
        # Offset in neuem Binary = wo wir sie ablegen
        new_off = len(result)
        result += file_data
        e['new_offset'] = new_off
        e['new_size']   = len(file_data)

    # WTG-Dateien einbauen
    for e in data_entries:
        name = e['name']
        if name in replacements:
            file_data = replacements[name]
            print(f"  ERSETZT: {name} ({e['size']} -> {len(file_data)} bytes)")
        elif e['size'] < 0x10000000:  # Gueltige Datei
            file_data = data[e['offset']:e['offset']+e['size']]
        else:
            print(f"  UEBERSPRUNGEN: {name} (ungueltige Groesse)")
            file_data = b''

        new_off = len(result)
        result += file_data
        e['new_offset'] = new_off
        e['new_size']   = len(file_data)

        # 4-Byte Ausrichtung
        pad = (4 - len(result) % 4) % 4
        result += b'\xff' * pad

    # Directory-Eintraege aktualisieren
    result = bytearray(result)
    for e in entries:
        if 'new_offset' not in e:
            continue
        dir_off = e['dir_off']
        # Offset und Size aktualisieren
        struct.pack_into('<I', result, dir_off + 20, e['new_offset'])
        struct.pack_into('<I', result, dir_off + 24, e['new_size'])
        # Checksum auf 0 setzen (Firmware prüft sie meistens nicht zur Laufzeit)
        # Wenn Probleme auftreten, Original-Checksum wiederherstellen:
        # struct.pack_into('<I', result, dir_off + 16, e['checksum'])

    return bytes(result)

def main():
    if len(sys.argv) < 2:
        print(f"Aufruf: python {sys.argv[0]} <neue_power_on.wtg>")
        print(f"Beispiel: python {sys.argv[0]} power_on_new.wtg")
        sys.exit(1)

    new_wtg_path = sys.argv[1]
    if not os.path.exists(new_wtg_path):
        print(f"FEHLER: Datei nicht gefunden: {new_wtg_path}")
        sys.exit(1)

    new_wtg_data = open(new_wtg_path, 'rb').read()
    print(f"Neue power_on.wtg: {len(new_wtg_data)} bytes")

    # tone.cfg lesen
    tone_data = open(TONE_CFG, 'rb').read()
    print(f"tone.cfg gelesen: {len(tone_data)} bytes")

    # Backup erstellen
    shutil.copy2(TONE_CFG, BACKUP)
    print(f"Backup erstellt: {BACKUP}")

    # Eintraege parsen
    entries = parse_entries(tone_data)
    print(f"\nDateien in tone.cfg ({len(entries)}):")
    for e in entries:
        if e['size'] < 0x10000000:
            print(f"  {e['name']:<20} offset=0x{e['offset']:06x} size={e['size']}")

    # Neue tone.cfg bauen
    print(f"\nErstelle neue tone.cfg...")
    replacements = {'power_on.wtg': new_wtg_data}
    new_tone = build_tone_cfg(tone_data, entries, replacements)

    # Speichern
    with open(TONE_CFG, 'wb') as f:
        f.write(new_tone)

    print(f"\n✓ tone.cfg aktualisiert: {len(new_tone)} bytes (war {len(tone_data)})")
    print(f"\nNaechste Schritte:")
    print(f"  1. download\\data_trans\\download.bat ausfuehren")
    print(f"  2. hybrid_firmware.bin neu erstellen (create_hybrid_firmware.ps1)")
    print(f"  3. Stift flashen")

if __name__ == '__main__':
    main()
