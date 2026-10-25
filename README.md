# TaubenKey  ·  von taubenopa

VST3 (Windows + macOS). Erkennt aus **Audio oder MIDI** die **Tonart**, die **Stimmung** (Cent-Abweichung von A=440, "detuned oder nicht") und das **Tempo (BPM)**.
Oberfläche: Nachtstadt-Beton-Look, animierter Taubenschwarm (Flügelschlag folgt dem BPM, Aufflattern bei neuer Tonart, Chef-Taube mit Krone + Sprechblase), klickbares Test-Klavier.

## Loslegen (GitHub)
1. Ordner in ein neues Repo pushen (Branch `main`).
2. Tab **Actions**: der Workflow `TaubenKey bauen` läuft automatisch (JUCE wird per CMake geholt).
3. Ergebnis unter **Artifacts**: `TaubenKey-Windows-Installer` (.exe), `TaubenKey-macOS-Installer` (.pkg).
4. Release: Tag `v1.0.0` pushen -> Installer landen automatisch im GitHub-Release.

## Lokal bauen
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target TaubenKey_VST3
```

## Analyse-Engine testen (ohne JUCE)
```
g++ -O2 -std=c++17 tests/analysis_test.cpp -o analysis_test && ./analysis_test
```

## Aufbau
| Datei | Inhalt |
|---|---|
| `Source/Analysis/Analysis.h` | Tonart (Chroma + KK/Temperley), Detune (zirkuläres Mittel), BPM (Flux + Autokorrelation) |
| `Source/PluginProcessor.*` | Audio-Tap, MIDI-Histogramm, Analyse-Thread, Test-Synth "Gurr-Ton" |
| `Source/PluginEditor.*` | Panels, Gauge, Chroma als Körnerhaufen |
| `Source/UI/*` | Taubenzeichnung, Schwarm, Klavier |
| `installer/` | Inno-Setup-Script, macOS-pkg, Artwork-Generator |

## Bedienung
- **Gedächtnis**: Länge des Analysefensters (6/12/20 s). **Einfrieren** hält das Ergebnis. **Neu schnüffeln** setzt zurück.
- **Klavier**: Klicken spielt "Gurr-Ton" (geht in die Analyse). **Test-Verstimmung** verstimmt diesen Ton um ±100 ct, um die Detune-Erkennung zu prüfen.
- MIDI-Noten (Host oder Klavier) haben Vorrang vor dem Audio-Spektrum bei der Tonart.
