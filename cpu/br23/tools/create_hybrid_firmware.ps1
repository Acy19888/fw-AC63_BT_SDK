# create_hybrid_firmware.ps1
# Kombiniert OEM-uboot mit unserem custom App-Code
# Ergebnis: hybrid_firmware.bin -> mit jl-uboot-tool flashen
#
# Aufbau:
#   0x00000 - 0x11FFF  : OEM uboot (aus oem_uboot.bin)
#   0x12000 - Ende     : Unser custom App (aus jl_isd.fw, Offset 0x400 + 0x12000)

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# Pfade
$OemUbootPath = Join-Path $ScriptDir "oem_uboot.bin"
$JlIsdPath    = Join-Path $ScriptDir "download\data_trans\jl_isd.fw"
$OutputPath   = Join-Path $ScriptDir "hybrid_firmware.bin"

# Pruefen ob Dateien vorhanden
if (-not (Test-Path $OemUbootPath)) {
    Write-Error "FEHLER: oem_uboot.bin nicht gefunden: $OemUbootPath"
    exit 1
}
if (-not (Test-Path $JlIsdPath)) {
    Write-Error "FEHLER: jl_isd.fw nicht gefunden: $JlIsdPath"
    Write-Host "Bitte zuerst download\data_trans\download.bat ausfuehren!"
    exit 1
}

Write-Host "=== Hybrid Firmware Builder ==="
Write-Host ""

# OEM uboot lesen (0x12000 = 73728 bytes)
$OemUboot = [IO.File]::ReadAllBytes($OemUbootPath)
Write-Host "OEM uboot gelesen: $($OemUboot.Length) bytes ($('{0:X}' -f $OemUboot.Length) hex)"

# jl_isd.fw lesen
$JlIsd = [IO.File]::ReadAllBytes($JlIsdPath)
Write-Host "jl_isd.fw gelesen: $($JlIsd.Length) bytes"

# jl_isd.fw hat einen 0x400-Header - der eigentliche Flash-Inhalt startet bei Byte 0x400
$FlashHeaderSize = 0x400
$AppOffset       = 0x12000   # App startet im Flash bei Adresse 0x12000

# App-Daten aus jl_isd.fw extrahieren: Offset 0x400 (Header) + 0x12000 (App-Adresse)
$AppStart = $FlashHeaderSize + $AppOffset
$AppData  = $JlIsd[$AppStart..($JlIsd.Length - 1)]
Write-Host "Custom App extrahiert: $($AppData.Length) bytes (ab jl_isd.fw Offset 0x$('{0:X}' -f $AppStart))"

# Sicherheitscheck: OEM uboot muss genau 0x12000 bytes sein
if ($OemUboot.Length -ne 0x12000) {
    Write-Error "FEHLER: oem_uboot.bin hat falsche Groesse! Erwartet: 73728, Gefunden: $($OemUboot.Length)"
    exit 1
}

# Hybrid zusammenbauen: OEM uboot + custom App
$HybridSize = $OemUboot.Length + $AppData.Length
$Hybrid = New-Object byte[] $HybridSize

[Array]::Copy($OemUboot, 0, $Hybrid, 0,                  $OemUboot.Length)
[Array]::Copy($AppData,  0, $Hybrid, $OemUboot.Length,   $AppData.Length)

Write-Host ""
Write-Host "=== Hybrid erstellt ==="
Write-Host "  0x00000 - 0x11FFF : OEM uboot ($($OemUboot.Length) bytes)"
Write-Host "  0x12000 - Ende    : Custom App ($($AppData.Length) bytes)"
Write-Host "  Gesamt            : $HybridSize bytes ($('{0:X}' -f $HybridSize) hex)"

# Speichern
[IO.File]::WriteAllBytes($OutputPath, $Hybrid)
Write-Host ""
Write-Host "Gespeichert: $OutputPath"

# Erste 32 Bytes anzeigen zur Verifikation
Write-Host ""
Write-Host "Erste 32 Bytes (OEM uboot Anfang):"
Write-Host ($Hybrid[0..31] | ForEach-Object { '{0:X2}' -f $_ }) -join ' '

Write-Host ""
Write-Host "Bytes bei 0x12000 (App Anfang):"
Write-Host ($Hybrid[0x12000..0x1200F] | ForEach-Object { '{0:X2}' -f $_ }) -join ' '

Write-Host ""
Write-Host "=== Naechster Schritt: Flashen ==="
Write-Host "Geraet in UBOOT-Modus bringen (Stecker ziehen, Knopf halten, Stecker rein)"
Write-Host "Dann ausfuehren:"
Write-Host ""
Write-Host "  jl-uboot-tool erasechip"
Write-Host "  jl-uboot-tool write hybrid_firmware.bin 0"
Write-Host "  jl-uboot-tool reset"
Write-Host ""
Write-Host "Fertig!"
