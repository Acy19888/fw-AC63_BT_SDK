# create_hybrid_v2.ps1
# Hybrid-Firmware v2: OEM-Chip-Header + SDK-uboot + SDK-App
#
# Layout:
#   0x00000 - 0x000BF  : OEM Chip-Info-Header (korrekte AC695N-Infos, ENTRY=0x1D000C0)
#   0x000C0 - 0x11FFF  : SDK uboot.boot (ROM springt hierher) + Nullen-Padding
#   0x12000 - Ende     : SDK Custom App (aus jl_isd.fw)
#
# Warum v2?  Der OEM-uboot (v1) versteht unser SDK-App-Format nicht.
#            Mit SDK-uboot werden OEM-Chip-Header UND SDK-App-Format korrekt kombiniert.

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

$OemUbootPath = Join-Path $ScriptDir "oem_uboot.bin"
$SdkUbootPath = Join-Path $ScriptDir "uboot.boot"
$JlIsdPath    = Join-Path $ScriptDir "download\data_trans\jl_isd.fw"
$OutputPath   = Join-Path $ScriptDir "hybrid_v2.bin"

foreach ($f in @($OemUbootPath, $SdkUbootPath, $JlIsdPath)) {
    if (-not (Test-Path $f)) {
        Write-Error "FEHLER: Datei nicht gefunden: $f"
        if ($f -eq $JlIsdPath) { Write-Host "  -> Zuerst download\data_trans\download.bat ausfuehren!" }
        exit 1
    }
}

Write-Host "=== Hybrid Firmware Builder v2 ==="
Write-Host ""

# OEM Chip-Info-Header: erste 0xC0 Bytes aus oem_uboot.bin
$OemFull   = [IO.File]::ReadAllBytes($OemUbootPath)
$ChipHdrSz = 0xC0
$ChipHdr   = $OemFull[0..($ChipHdrSz - 1)]
Write-Host "OEM Chip-Header gelesen: $ChipHdrSz bytes (0x$($ChipHdrSz.ToString('X')))"
Write-Host "  Erste 16 Bytes: $(($ChipHdr[0..15] | ForEach-Object { '{0:X2}' -f $_ }) -join ' ')"

# Pruefe AC695N-Kennung
$ChipName = [System.Text.Encoding]::ASCII.GetString($ChipHdr[0x10..0x1E]) -replace '[^\x20-\x7E]',''
Write-Host "  Chip-Kennung: $ChipName"

# SDK uboot.boot
$SdkUboot = [IO.File]::ReadAllBytes($SdkUbootPath)
Write-Host ""
Write-Host "SDK uboot.boot gelesen: $($SdkUboot.Length) bytes (0x$($SdkUboot.Length.ToString('X')))"

# SDK App aus jl_isd.fw extrahieren (Offset 0x400 = fw-Header, + 0x12000 = App-Startadresse)
$JlIsd    = [IO.File]::ReadAllBytes($JlIsdPath)
$AppStart = 0x400 + 0x12000
$AppData  = $JlIsd[$AppStart..($JlIsd.Length - 1)]
Write-Host "SDK App extrahiert: $($AppData.Length) bytes (ab jl_isd.fw Offset 0x$($AppStart.ToString('X')))"

# Sicherheitschecks
if ($OemFull.Length -lt $ChipHdrSz) {
    Write-Error "FEHLER: oem_uboot.bin zu klein!"; exit 1
}
if (($ChipHdrSz + $SdkUboot.Length) -gt 0x12000) {
    Write-Error "FEHLER: uboot.boot ($($SdkUboot.Length) bytes) passt nicht in uboot-Bereich!"; exit 1
}

# Firmware zusammenbauen
$TotalSz = 0x12000 + $AppData.Length
$Fw = New-Object byte[] $TotalSz

# Block 1: OEM Chip-Header (0x000 - 0x0BF)
[Array]::Copy($ChipHdr,   0, $Fw, 0x000,     $ChipHdr.Length)

# Block 2: SDK uboot.boot (0x0C0 - 0x0C0+len), Rest Nullen
[Array]::Copy($SdkUboot,  0, $Fw, 0x0C0,     $SdkUboot.Length)
# (Nullen von 0x0C0+uboot.Length bis 0x11FFF sind bereits in New-Object)

# Block 3: SDK App (0x12000 - Ende)
[Array]::Copy($AppData,   0, $Fw, 0x12000,   $AppData.Length)

Write-Host ""
Write-Host "=== Hybrid v2 erstellt ==="
Write-Host "  0x00000 - 0x000BF : OEM Chip-Header ($ChipHdrSz bytes)"
Write-Host "  0x000C0 - 0x$('{0:X5}' -f (0x0C0 + $SdkUboot.Length - 1)) : SDK uboot.boot ($($SdkUboot.Length) bytes)"
Write-Host "  0x$('{0:X5}' -f (0x0C0 + $SdkUboot.Length)) - 0x11FFF : Null-Padding ($('{0:X}' -f (0x12000 - 0x0C0 - $SdkUboot.Length)) bytes)"
Write-Host "  0x12000 - Ende    : SDK App ($($AppData.Length) bytes)"
Write-Host "  Gesamt            : $TotalSz bytes ($('{0:X}' -f $TotalSz) hex)"

[IO.File]::WriteAllBytes($OutputPath, $Fw)
Write-Host ""
Write-Host "Gespeichert: $OutputPath"

Write-Host ""
Write-Host "Erste 32 Bytes (OEM Chip-Header Anfang):"
Write-Host (($Fw[0..31] | ForEach-Object { '{0:X2}' -f $_ }) -join ' ')
Write-Host ""
Write-Host "Bytes bei 0x0C0 (SDK uboot.boot Anfang):"
Write-Host (($Fw[0xC0..0xCF] | ForEach-Object { '{0:X2}' -f $_ }) -join ' ')
Write-Host ""
Write-Host "Bytes bei 0x12000 (SDK App Anfang):"
Write-Host (($Fw[0x12000..0x1200F] | ForEach-Object { '{0:X2}' -f $_ }) -join ' ')

# Kurze Kopie erstellen fuer einfacheres Flashen (kein Leerzeichen im Pfad)
$ShortPath = "C:\fw_hybrid.bin"
try {
    [IO.File]::WriteAllBytes($ShortPath, $Fw)
    Write-Host ""
    Write-Host "Kopie gespeichert: $ShortPath  (kuerzerer Pfad zum Flashen)"
} catch {
    Write-Host ""
    Write-Host "HINWEIS: Kopie nach C:\fw_hybrid.bin fehlgeschlagen (kein Schreibrecht?)"
    Write-Host "  -> Bitte manuell kopieren: copy `"$OutputPath`" C:\fw_hybrid.bin"
}

Write-Host ""
Write-Host "=== Naechster Schritt: Flashen ==="
Write-Host "Geraet in UBOOT-Modus (Knopf halten + USB einstecken)"
Write-Host "jl-uboot-tool starten, dann im =>JL: Prompt:"
Write-Host ""
Write-Host "  erasechip"
Write-Host "  write 0x0 C:\fw_hybrid.bin"
Write-Host "  reset"
Write-Host ""
Write-Host "Fertig!"
