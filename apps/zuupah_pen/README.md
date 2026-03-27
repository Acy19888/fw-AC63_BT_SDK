# Zuupah Pen Firmware

Firmware für den Zuupah Lernstift (JieLi AC6956C8 / br23 platform)

## Features

- **BLE 5.1** — Batterie %, freier Speicher, aktuelles Buch, Events an App
- **SPP (BR/EDR)** — Schneller Dateitransfer (~700 KB/s) von der App auf den Stift
- **Auto-Sleep** — Sprachwarnung nach 5 Min, Power-Off nach 60 Sek
- **Audio-Wiedergabe** — MP3 von 8GB Flash

---

## Schritt 1: JieLi Toolchain installieren

### Windows (empfohlen)
1. Gehe zu: http://pkgman.jieliapp.com/doc/all
2. Lade die Windows Toolchain herunter
3. Entpacke nach `C:\JL\`
4. Stelle sicher dass `C:\JL\pi32\bin\clang.exe` existiert

### Mac/Linux
```bash
# Toolchain herunterladen von http://pkgman.jieliapp.com/doc/all
# Nach /opt/jieli/ entpacken
sudo mkdir -p /opt/jieli
sudo tar -xf jieli_toolchain.tar.gz -C /opt/jieli/

# Prüfen ob clang verfügbar ist
/opt/jieli/pi32v2/bin/clang --version
```

---

## Schritt 2: Kompilieren

```bash
# Im Zuupah Makefile-Ordner:
cd fw-AC63_BT_SDK/apps/zuupah_pen/board/br23/

# Firmware bauen
make

# Mit detailliertem Output
make VERBOSE=1

# Aufräumen
make clean
```

**Ergebnis:** `cpu/br23/tools/sdk.ufw` — das ist deine Firmware-Datei!

---

## Schritt 3: Firmware auf Stift flashen

### Mit ISD Download Tool (Windows)
1. Lade ISD Download Tool von JieLi herunter
2. Stift in Download-Mode versetzen:
   - **Methode A:** Stift einschalten → Tool erkennt automatisch
   - **Methode B:** UBOOT-Pin auf GND beim Einstecken (braucht Lötkolben)
3. `sdk.ufw` im Tool öffnen
4. **Download** klicken → ~30 Sekunden → Fertig ✓

---

## Dateistruktur

```
apps/zuupah_pen/
├── board/
│   └── br23/
│       └── Makefile          ← Build-Konfiguration
├── examples/
│   └── zuupah/
│       ├── zuupah_main.c     ← Haupt-Logik (Sleep, Batterie, Audio)
│       ├── zuupah_ble.c      ← BLE GATT Server
│       ├── zuupah_ble.h
│       ├── zuupah_ble_profile.h  ← BLE Service UUIDs
│       ├── zuupah_spp.c      ← SPP Dateiempfang
│       └── zuupah_spp.h
└── README.md
```

## Flash-Ordnerstruktur (8GB)

```
ZUUPAH/
├── BOOKS/
│   ├── dschungelbuch.mp3
│   ├── dschungelbuch.mp3.meta
│   └── ...
└── SOUNDS/
    ├── startup.mp3
    ├── shutdown_warning.mp3
    ├── goodbye.mp3
    ├── low_battery.mp3
    ├── book_received.mp3
    └── ...
```

## BLE UUIDs (für App-Entwickler)

| Characteristic | UUID | Typ |
|---|---|---|
| Battery % | `0000AA01-...` | READ + NOTIFY |
| Storage Free MB | `0000AA02-...` | READ + NOTIFY |
| Storage Total MB | `0000AA03-...` | READ |
| Current Book ID | `0000AA04-...` | READ + NOTIFY |
| Pen Event | `0000AA05-...` | NOTIFY |

## SPP Transfer-Protokoll

App → Pen:
```
[4B Magic "ZUUP"][4B totalBytes][4B CRC32][4B filenameLen][N Bytes filename][Dateidaten...]
```

Pen → App:
- `"OK\n"` — Bereit für Empfang
- `"DONE\n"` — Erfolgreich gespeichert
- `"ERROR:...\n"` — Fehler
