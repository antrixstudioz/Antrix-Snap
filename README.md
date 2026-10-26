# Snap — Antrix Studioz

An animated finger-snap instrument for Windows (VST3) and macOS (universal VST3 / AU), plus standalone apps.

## First prototype
Black and gold artwork follows the supplied design reference. Clicking the hand runs an eight-pose animation; the click preview fires at the palm-strike frame (~105 ms). MIDI triggers audio immediately to preserve timing, and the animation starts at the impact phase. Each hit is procedurally synthesized with noise, a short resonant body and randomized character. This is not a recorded 50-sample library.

Controls: Tone, Humanize, Pitch, Velocity, Space, Width and Level. Single, Double, Triple and Flam bursts. Natural, Soft, Sharp, Deep, Bright, Airy and Random styles. Any MIDI note triggers a one-shot; MIDI velocity affects level. DAW automation and parameter recall are supported.

Artwork and animation are a first implementation, not a verified 90–95% visual match. Frame registration and finger anatomy need visual review in the running plugin.

## Downloads
Open **Actions → Build Snap → latest successful run → Artifacts**. Windows and macOS use different binaries. Build success is not equivalent to a DAW compatibility test.

Windows: copy the entire Snap.vst3 bundle to `C:\\Program Files\\Common Files\\VST3` and rescan plugins.
macOS: unzip the inner VST3 or AU ZIP; copy Snap.vst3 to `~/Library/Audio/Plug-Ins/VST3` or Snap.component to `~/Library/Audio/Plug-Ins/Components`. Builds are unsigned/not notarized development builds.
Load Snap as an instrument on a MIDI track.

## Build
Requires CMake 3.22+, C++17 and an appropriate native compiler (Visual Studio 2022 or Xcode). CMake downloads JUCE 8.0.6.
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 3
ctest --test-dir build -C Release --output-on-failure
```
Core DSP tests need no external dependencies: configure with `-DSNAP_TESTS_ONLY=ON`.

## Licensing
JUCE is a third-party dependency with its own AGPLv3/commercial licensing terms. No commercial distribution license is granted by this repository. Confirm JUCE licensing before distributing a proprietary release. User-provided reference artwork and generated animation assets are included for this project.

