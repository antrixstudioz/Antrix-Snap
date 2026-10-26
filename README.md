# Snap — Antrix Studioz

Snap 0.3 is a compact finger-snap instrument for Windows VST3 and macOS universal VST3/AU, with standalone apps.

## Play and keep your snaps
- Click the hand to create a new snap. Every generated hit is rendered and saved as a stereo 24-bit, 48 kHz WAV in the background.
- Click the waveform, or press Space while the editor has keyboard focus, to replay the selected recording exactly. Replay does not randomize or create a new history entry.
- Hold and drag the waveform into a DAW or file browser to copy its WAV. The original remains in history.
- Choose an earlier item in History or use the arrow buttons to replay older snaps. Folder opens the saved WAV directory. There is no automatic history deletion.
- Press A W S E D F T G Y H U J K for new snaps, or click the on-screen piano. Computer typing needs the plugin editor focused; a DAW may reserve keys until its typing-keyboard mode is disabled.
- Any MIDI note triggers a new snap with MIDI velocity. In a DAW, route/arm a MIDI track to Snap. The standalone app has a MIDI Input selector and selects the first available device initially.

## Sound
The embedded bank contains 29 owner-supplied real snap recordings. Each hit blends a dominant recording with a quieter second recording. Natural cycles through the full bank without immediate primary repeats. Soft/Deep use darker sample pools; Sharp uses shorter recordings; Bright uses brighter recordings; Airy uses longer tails with more width. Each style also has distinct filtering, pitch or envelope settings. Random chooses a style per hit.

Punch strengthens the attack without adding a synthetic snare/rim body. Tone, Humanize, Pitch, Velocity, Width and Level remain available. Reverb is now an actual diffuse reverb, separate from the stereo Delay and its Time control. Single, Double, Triple and Flam modes are retained.

New generations use the current controls. History and waveform playback reproduce the frozen WAV, including the effects settings used at creation; changing controls does not overwrite old sounds. Overlapping MIDI notes share the live effects bus, while each saved WAV is rendered as its own isolated hit.

## Interface
The default window is 780×520 (minimum 690×460). Decorative header/footer text boxes have been removed. Hand sway, finger bending, eased pose blends, gold rings and drifting particles animate continuously at 60 Hz; a hit adds a stronger recoil/flash. The hand's hit target draws no border. The editor can still be resized.

## History location
- Windows: `%APPDATA%\Antrix\Snap\History`
- macOS: `~/Library/Antrix/Snap/History`

Use Folder in the plugin for the exact path. WAVs are shared by plugin instances on the same computer and survive closing the editor, restarting the plugin and reopening a DAW. Project state remembers the selected WAV name, but moving the project to another computer also requires copying the WAV history. The plugin does not upload recordings.

History rendering and disk writes run outside the audio callback. Closing the processor waits for queued saves. Disk failures or an extreme recording-queue overflow are shown in the UI; clips cannot be guaranteed saved if storage is unavailable. Saved files are not silently evicted.

## Downloads and installation
Open **Actions → Build Snap → latest successful run → Artifacts**. Windows and macOS binaries are separate. Each build also includes UI preview images.

Close Snap/the host before replacing an older build. Windows: copy the entire `Snap.vst3` bundle into `C:\Program Files\Common Files\VST3`, then rescan plugins. macOS: extract the inner archive and copy `Snap.vst3` into `~/Library/Audio/Plug-Ins/VST3` or `Snap.component` into `~/Library/Audio/Plug-Ins/Components`. macOS builds are development builds without notarization. Standalone apps are included.

Existing sessions retain saved parameter values. Start with Natural, Pitch 0 st, Tone 55%, Width 50%, and Reverb/Delay 0% for the dry reference sound.

## Build and validation
Requires CMake 3.22+, C++17 and Visual Studio/Xcode. CMake downloads JUCE 8.0.6.
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 3
ctest --test-dir build -C Release --output-on-failure
```
`-DSNAP_TESTS_ONLY=ON` builds core DSP tests without JUCE. Native CI also checks host-MIDI sample timing, computer-keyboard sound, exact preview replay, WAV export validity, history persistence and restored selection, dry-path unity, delay/reverb tails, compact dimensions, transparent hand hit area and idle animation. It renders screenshots of the actual editor. Hardware MIDI routing and file drops into individual DAWs still require host-specific testing.

## Assets and licensing
Sample preparation: `tools/prepare_samples.py`. Sample provenance and rights: `Assets/SAMPLE-NOTICE.md`. Renaming sample identifiers does not change the original recordings' licenses. JUCE has its own AGPLv3/commercial licensing terms; this repository does not grant a JUCE commercial license or third-party sample redistribution rights.
