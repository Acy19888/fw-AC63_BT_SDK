@echo off
REM build_custom_tone.bat
REM Konvertiert startup_8k.wav zu .wtg und baut neue tone.cfg
REM Ausfuehren aus: cpu\br23\tools\

setlocal
cd /d "%~dp0"

echo === Custom Tone Builder ===
echo.

REM Pfade
set WTG_ENCODE=AC695X_config_tool\conf\source\tone\wtg_encode.exe
set WTG_DECODE=AC695X_config_tool\conf\source\tone\wtg_decode.exe
set INPUT_WAV=startup_8k.wav
set OUTPUT_WTG=power_on_custom.wtg

REM Pruefen
if not exist "%WTG_ENCODE%" (
    echo FEHLER: wtg_encode.exe nicht gefunden: %WTG_ENCODE%
    pause
    exit /b 1
)
if not exist "%INPUT_WAV%" (
    echo FEHLER: %INPUT_WAV% nicht gefunden
    echo Bitte erst startup_8k.wav in diesen Ordner kopieren
    pause
    exit /b 1
)

REM Schritt 1: WAV -> WTG konvertieren
echo [1/3] Konvertiere %INPUT_WAV% -> %OUTPUT_WTG%...
"%WTG_ENCODE%" "%INPUT_WAV%" "%OUTPUT_WTG%"
if not exist "%OUTPUT_WTG%" (
    echo FEHLER: wtg_encode hat keine Ausgabe erzeugt
    REM Versuche alternatives Format (manche wtg_encode brauchen andere Syntax)
    echo Versuche alternatives Format...
    "%WTG_ENCODE%" -i "%INPUT_WAV%" -o "%OUTPUT_WTG%"
)
if not exist "%OUTPUT_WTG%" (
    echo FEHLER: Konvertierung fehlgeschlagen
    echo Versuche manuell: %WTG_ENCODE% %INPUT_WAV% %OUTPUT_WTG%
    pause
    exit /b 1
)

for %%A in ("%OUTPUT_WTG%") do echo OK: %OUTPUT_WTG% (%%~zA bytes)

REM Schritt 2: tone.cfg patchen
echo.
echo [2/3] Patche tone.cfg (ersetzt power_on.wtg)...
python patch_tone_cfg.py "%OUTPUT_WTG%"
if errorlevel 1 (
    echo FEHLER: patch_tone_cfg.py fehlgeschlagen
    pause
    exit /b 1
)

REM Schritt 3: Firmware neu bauen
echo.
echo [3/3] Firmware paketieren...
cd download\data_trans
call download.bat
cd ..\..

echo.
echo === Fertig! ===
echo Naechster Schritt: create_hybrid_firmware.ps1 ausfuehren
echo Dann hybrid_firmware.bin flashen.
echo.
pause
