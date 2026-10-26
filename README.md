# Snap — Antrix Studioz

An animated finger-snap instrument for Windows (VST3) and macOS (universal VST3 / AU), plus standalone apps.

## Version 0.2 — real sample playback
Black and gold artwork follows the supplied design reference. Clicking the hand runs an eight-pose animation; the click preview fires at the palm-strike frame (~105 ms). MIDI triggers audio immediately to preserve timing, and the animation starts at the impact phase. Each hit blends two real finger-snap recordings from the 29 owner-supplied references. The main recording stays dominant; a quieter, onset-aligned second recording changes its texture. There is no synthetic oscillator/noise layer. A shuffled bag uses every primary sample once before reshuffling and prevents adjacent primary repeats, including at bag boundaries. Humanize adds subtle pitch, gain and blend variation. This is a 29-recording library, not a 50-sample library.

The hand trigger paints no outline or hover/focus box. Knobs remain circular at the minimum editor size, values use readable percentages/semitones, and the waveform holds the most recent hit. The audio bank is embedded in the plugin; no external sample folder is required.

For the natural reference sound, use Natural, Pitch 0 st, Tone 55%, Space 0%, Width 50%. Existing DAW sessions retain their saved controls, so reset extreme Pitch/Tone values when comparing the new engine.

Controls: Tone, Humanize, Pitch, Velocity, Space, Width and Level. Single, Double, Triple and Flam bursts. Natural, Soft, Sharp, Deep, Bright, Airy and Random styles. Any MIDI note triggers a one-shot; MIDI velocity affects level. DAW automation and parameter recall are supported.

Artwork and animation are a first implementation, not a verified 90–95% visual match. Frame registration and finger anatomy need visual review in the running plugin.

## Downloads
Open **Actions → Build Snap → latest successful run → Artifacts**. Windows and macOS use different binaries. Build success is not equivalent to a DAW compatibility test.

Windows: copy the entire Snap.vst3 bundle to `C:\Program Files\Common Files\VST3` and rescan plugins.
macOS: unzip the inner VST3 or AU ZIP; copy Snap.vst3 to `~/Library/Audio/Plug-Ins/VST3` or Snap.component to `~/Library/Audio/Plug-Ins/Components`. Builds are unsigned/not notarized development builds.
Load Snap as an instrument on a MIDI track.

## Build
Requires CMake 3.22+, C++17 and an appropriate native compiler (Visual Studio 2022 or Xcode). CMake downloads JUCE 8.0.6.
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 3
ctest --test-dir build -C Release --output-on-failure
```
Core DSP tests need no external dependencies: configure with `-DSNAP_TESTS_ONLY=ON`. They verify the real sample bank, shuffle coverage, distinct layers, PCM-only output, rapid retriggers and tails at 44.1/48/96 kHz. Native builds also render `Snap-UI.png` and assert that the hand hit target is completely transparent.

The bank preparation tool is `tools/prepare_samples.py`. Its input is the owner-provided WAV directory. It converts to 48 kHz stereo PCM16, removes leading silence, aligns attacks, normalizes peaks, fades release edges, and bounds tails to 1.2 seconds. Bank entries are named Snap_001 through Snap_029 in `Assets/samples.json`.

## Licensing
JUCE is a third-party dependency with its own AGPLv3/commercial licensing terms. No commercial distribution license is granted by this repository. Confirm JUCE licensing before distributing a proprietary release. User-provided reference artwork and generated animation assets are included for this project.

