# VariMu 660

En enkel variable-mu-kompressor i VST3-format (og AU på Mac), inspirert av Fairchild 660/670. Bygget med JUCE 9.0.5.

![Skjermbilde](screenshot.png)

## Kontroller

| Kontroll | Hva den gjør |
|---|---|
| **Input Gain** (-12 til +24 dB) | Hvor hardt signalet drives inn i kompressoren og røret. Som på originalen: mer input = mer kompresjon og mer farge. |
| **Threshold** (0–10) | Terskel. Høyere tall = lavere terskel = mer kompresjon (fra ca. -4 til -36 dBFS). |
| **Time Constant** (1–6) | De seks klassiske attack/release-kombinasjonene (se under). |
| **Output** (-24 til +24 dB) | Utgangsnivå/makeup. |
| **Tube Color** (0–100 %) | Mengde rør- og transformatormetning. 0 % = ren kompresjon. |
| **Mix** | Parallellkompresjon (dry/wet, latenskompensert). |
| **Stereo Link** | Felles gain-reduksjon for venstre/høyre (ingen stereobilde-vandring). Av = dual mono. |

### Time Constant

| Pos | Attack | Release |
|---|---|---|
| 1 | 0,2 ms | 0,3 s |
| 2 | 0,2 ms | 0,8 s |
| 3 | 0,4 ms | 2 s |
| 4 | 0,4 ms | 5 s |
| 5 | 0,4 ms | Auto: ca. 2 s for enkelttopper, opp mot 10 s ved vedvarende kompresjon |
| 6 | 0,2 ms | Auto: ca. 0,3 s for enkelttopper, 10 s for flere topper, opp mot 25 s ved vedvarende kompresjon |

Typisk bruk: 1–2 på trommer og bass, 3 på vokal, 4–6 på buss og mastering (lav gain-reduksjon, 1–3 dB).

## Hvordan modellen fungerer

- **Feedback-topologi**: sidechain leser utgangen av gain-reduksjonstrinnet, som i originalen.
- **Progressiv ratio**: ca. 2:1 like over terskel, øker gradvis mot ~20:1 jo hardere du kjører den, med myk knee (12 dB). Dette er variable-mu-rørets kjennetegn.
- **Auto-release (5 og 6)**: modellert som et RC-nettverk med flere kondensatorer. De trege kondensatorene lades bare når kompressoren jobber over tid, så enkelttopper slippes raskt mens vedvarende kompresjon slippes sakte.
- **Rørfarge**: asymmetrisk metning (mest 2. harmonisk) der forvrengningen øker med gain-reduksjonen, pluss myk transformatormetning i bassen.
- **2x oversampling** (lineær fase) på hele kompressoren for å unngå aliasing. Latensen (ca. 1 ms) rapporteres til DAW-en.

## Kodestruktur

| Fil | Innhold |
|---|---|
| `Source/VariMuCompressor.h` | Selve kompressoren og rørfargen. Ren C++ uten JUCE, så den kan testes alene. |
| `Source/PluginProcessor.*` | Parametere, input/output-gain (glattet), oversampling, dry/wet og bypass. |
| `Source/PluginEditor.*` | GUI: knotter, VU-måler og Fairchild-panelet. |
| `Tests/` | DSP-test, VST3-testvert og skjermbilde av GUI-et. |

Koden følger vanlig praksis for VST3/JUCE:

- Ingen minneallokering, låser eller fil-I/O i lydtråden. Alt settes opp i `prepareToPlay`.
- Input- og output-gain glattes over 50 ms, så automasjon ikke gir knitring.
- Latensen er fast og rapporteres til verten. Bypass forsinker lyden like mye, så den ikke hopper i tid.
- Tomme lydblokker og varierende blokkstørrelser håndteres.

## Lisens

JUCE er dobbeltlisensiert under AGPLv3 og en kommersiell JUCE-lisens. Til privat bruk er det uproblematisk. Skal pluginen deles som lukket kode eller selges, kan du trenge en kommersiell lisens. Se [JUCE-lisensen](https://juce.com/legal/juce-9-licence/).

## Bygge på Mac

Du trenger Xcode (fra App Store) og CMake (`brew install cmake`).

```bash
cd VariMu660
cmake -B build -G Xcode
cmake --build build --config Release
```

JUCE lastes ned automatisk første gang (tar et par minutter). Resultatet havner i:

- `build/VariMu660_artefacts/Release/VST3/VariMu 660.vst3`
- `build/VariMu660_artefacts/Release/AU/VariMu 660.component`
- `build/VariMu660_artefacts/Release/Standalone/VariMu 660.app` (for rask testing uten DAW)

Installer ved å kopiere til:

```bash
cp -R "build/VariMu660_artefacts/Release/VST3/VariMu 660.vst3" ~/Library/Audio/Plug-Ins/VST3/
cp -R "build/VariMu660_artefacts/Release/AU/VariMu 660.component" ~/Library/Audio/Plug-Ins/Components/
```

Pluginen er ikke signert, så hvis macOS blokkerer den:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"VariMu 660.vst3"
```

Universal binary (Apple Silicon + Intel) bygges automatisk.

### Alternativ: bygg via GitHub (uten Xcode)

Legg prosjektet i et GitHub-repo. Workflowen i `.github/workflows/build-macos.yml` bygger VST3 + AU på en Mac-server ved hver push, og ferdig zip kan lastes ned under *Actions → siste kjøring → Artifacts*.

## Tester

```bash
cmake -B build -DVARIMU_BUILD_TESTS=ON
cmake --build build --target DspTest && ./build/DspTest
```

`DspTest` måler statisk kompresjonskurve, release-tider for alle seks posisjoner og harmonisk innhold. Kjør gjerne [pluginval](https://github.com/Tracktion/pluginval) på Macen i tillegg.
