# Building the KICK app on a Mac

> **Provenance.** The stand-ins below come from the `macos-build` branch of
> [Eight4aWish/0700-AAM](https://github.com/Eight4aWish/0700-AAM/tree/macos-build) (AGPL-3.0, same
> project), merged here unchanged with its history. Changes made on top in this fork
> (ghstcoder47/0700-AAM) are listed under [Additions in this fork](#additions-in-this-fork).

This is the `macos-build` branch of [Eight4aWish/0700-AAM](https://github.com/Eight4aWish/0700-AAM),
a fork of [MusicTribeOpenSource/0700-AAM](https://github.com/MusicTribeOpenSource/0700-AAM). It is
not made, approved or supported by Music Tribe. `main` follows upstream unchanged.

The release does not build as published (issue #9): two modules the app includes, and the
CMake file that registers them, were not released; a third module was removed for licensing;
and AudIO ships with its recorder switched off. This branch fills those gaps so that the KICK's own app (`Kick`
target) builds and runs on macOS. It starts and shows its Projects screen at 1024 × 600.

Built on macOS (Apple Silicon) with Apple clang 21, CMake 4.0 and Ninja, against JUCE 7.0.12
with the release's own multitouch patch.

```bash
brew install cmake ninja   # if not installed yet
Kick/build-mac.sh          # configure + build the Kick target
open Kick/build-sim/Kick.app
```

The desktop simulator (Kick plus an on-screen control panel, see [Simulator](#simulator-kt_kickem)):

```bash
Kick/build-mac.sh KT_KickEM
open --env KICK_NO_AUDIO_INPUT=1 Kick/build-sim/KT_KickEM.app
```

## Additions in this fork

Verified on macOS 26.5 (Apple Silicon), Apple clang 21, CMake 4.4, Ninja 1.13.

- **Microphone permission.** The `Kick` target now declares `NSMicrophoneUsageDescription`.
  Without it the app froze on start inside CoreAudio (black window), because opening the input
  device waits on macOS's microphone permission. On first launch, and again after every rebuild
  (the ad-hoc signature changes), macOS asks; the window stays black until you answer.
  `KICK_NO_AUDIO_INPUT=1` opens the audio device output-only and skips the prompt:
  `open --env KICK_NO_AUDIO_INPUT=1 Kick/build-sim/Kick.app`.
- **Working directory.** On macOS `kick_paths.cpp` looks for `./assets`, `./demos`, `./templates`,
  `./projects`, ... relative to the working directory. CMake copies assets, demos and templates
  into `Kick/build-sim`, but `open` starts apps in `/`, so the UI came up empty. If `./assets`
  is missing, the app now changes to the folder holding `Kick.app` (macOS only). Projects, logs and
  prefs are therefore written to `Kick/build-sim/`.
- **Audio fixes** (bugs in the upstream code, not in the stand-ins):
  - *Feedback on project load.* `AudioObject.h` declared the object-ID counter `static` in a
    header, so every translation unit had its own counter and IDs collided. A synth object got
    the input port's ID, and its routing then wired the microphone to the outputs and sends.
    The counter is now a single `inline std::atomic`.
  - *Preview starts mid-sample.* `audioPreview` faded in over 1 s, so the start of a sample
    was inaudible. The fade is now 10 ms.
  - *Sampler waveform stays flat.* The recorder thumbnail always used the same cache hash and
    kept showing the empty picture cached right after REC. The cached entry is now dropped
    before each refresh. The recording itself was fine.
- **Sampling tips.** Your own samples go in `Kick/build-sim/data/samplesKick/`; there is no
  factory content. The Sampler's fader is the record threshold (default about −14 dB):
  recording starts once the input exceeds it. Use headphones with Monitor on, or the
  speakers feed back into the microphone.
- **Drum grid in the Step Sequencer** (`kick_DrumGrid.{h,cpp}`). STEP SEQ now opens a grid with
  the 16 pads of the selected pad's bank as rows and the steps of the current page as columns.
  The GRID/CLASSIC button in the header switches back to the original view. Pad, bar, page and
  time division come from the existing controls.
  - *NOTES layer:* tap sets or clears a step; a swipe along a row paints or erases.
  - *VELOCITY / PROBABILITY / RATCHET layers:* each cell is a small fader. Tapping an empty step
    creates the note at the tapped height; tapping a note selects only that note; a swipe draws
    values. With the SHIFT button held or latched, a tap adds or removes a note from the
    selection and dragging adds a rectangle (across pads and steps); SHIFT + tap on an empty
    step clears the selection.
  - *Dial and + / −:* change all selected notes relatively (each stops at its limit); with
    nothing selected (always in NOTES) they nudge the selected pad. Tapping the grid releases the
    keyboard focus of other controls so the dial reaches the grid.
  - Tapping a pad name selects that pad. The playing step is highlighted.
- **Step Sequencer changes** (both views):
  - In CLASSIC, moving a value slider on an empty step now creates the note.
  - Nudge wraps the pad's notes around inside the sequence. The engine's `nudgeNotes()` erased
    notes pushed before the start and let notes run past the end.
  - Turning a step off with the step buttons erased the notes of every pad on that step; now it
    erases only the selected pad's note.
- **Grid View pencil tool, SHIFT shortcuts** (`kick_GridPencilTool.{h,cpp}`). With the SHIFT button
  held or latched: a tap on a note deletes it, dragging from an empty spot selects the notes in a
  rectangle, and a tap on an empty spot clears the selection. Dragging a note still moves it.
  Without SHIFT nothing changes.

### Simulator (`KT_KickEM`)

`KT_KickEM` (`-DKICK_SIMULATOR=ON`, which `build-mac.sh` sets) runs the full Kick app, started
the same way as `apps/kick/Main.cpp`, with an on-screen control surface in place of the serial
panel link. The old panel code predated the current packet protocol and its image assets are
no longer in the release, so it was rewritten with plain JUCE widgets:

- `Kick/apps/kickem/simulator/kick_Simulator.{h,cpp}`: 16 pads, 16 Q-link encoders, the
  centre encoder (push = ENTER), mode, bank, transport, level and navigation buttons, and SHIFT.
  The panel draws the pad colours and button LEDs that the app sends back.
- `Kick/modules/input_events/kick_SimulatorInterface.h` and `input_events.cpp`: with
  `SIM_BUILD=1` (simulator target only) no serial port is opened. Controls inject
  `CPktMgr::HWMsg_t` packets through `Controls::InputEvents::dispatchHardwareEvent`, the same
  path as serial input. Outgoing pad and LED packets go to a `SimulatorInterface::messageToHardware()`
  handler instead of the port. The `Kick` target is unchanged.
- The old `kick_SimulatorMainControl`, `kick_SimulatorControls` and `kick_SimulatorInterfaceImpl`
  were removed.

Operating the panel:

- Encoders: drag up/down (6 px per step) or use the mouse wheel; clicking the centre encoder
  sends ENTER.
- SHIFT latches for one press. Holding the keyboard's Shift key also works. Shifted functions
  (GRID, SAVE, bank E–H, ...) are labelled under each button and send their own button IDs,
  as the app expects.

The panel is 1484 × 900 and is scaled down to fit the screen. `KICK_SIM_SCALE=<factor>`
overrides the scale.

Simplifications, from reading the app rather than the hardware:

- Pads are hit at a fixed velocity of 100 with no aftertouch.
- The master/rec knobs and the slider are not simulated; the app does not consume slider
  packets.
- How the real panel combines LEDs is a guess: bank LEDs and PLAY/STOP/PLAY START are shown
  as radio groups.

## What was missing, and what stands in for it

Everything below is written from how Kick uses each piece: the names, types and call sites
in the released code. None of it is Music Tribe's original, and where a value is a guess
the header says so.

| Missing piece | Stand-in | Notes |
| --- | --- | --- |
| `AudIO/juce_modules.cmake` | registers `AudIO`, `AudIO_constants`, `AudIO_juce` | three lines |
| `AudIO_constants` module | `MIDI`, `MIDI_CH`, `POLYPHONY`, `GAIN`, `PAN`, `DB` ranges, `UNKNOWN_VALUE`, the engine's sample rate, block size and bus counts | ranges follow `kick_Constants.h`; guesses are marked |
| `AudIO_juce` module | includes AudIO and the JUCE audio modules, and supplies the classes below | the `AUDIO_JUCE_EXTENSIONS` block in `AudIO.h` is empty in the release |
| `SampleRecorder`, `Looper` | thin subclasses of AudIO's published `DRecorder` / `AudioRecorder`, under the names Kick calls | looper undo/redo are no-ops |
| `mt::AudioDevice` | AudIO's `IAudioDevice` driven by `juce::AudioDeviceManager` | |
| `mt::Filter`, `FilterType` | a TPT state-variable filter covering the list in `kick_Constants.h` | pole counts approximated; "Model" types fall back to low-pass |
| `mt::ADSR` five-argument `setParameters` | a subclass of AudIO's `ADSR<float>` | reorders to AudIO's `Params` |
| `juce_serialport` (removed: no licence) | a POSIX termios serial port with JUCE streams | the panel link on the device |

Smaller changes: AudIO's `AUDIO_OBJECT_AUDIO_FILE_RECORDER` switched on (Kick's recorder
needs it); `<thread>` and `<sys/stat.h>` included where libc++ and macOS need them; the
simulator target generates `config.h` like the other apps.

## Linux

Confirmed by HeyMoustache in issue #9 (27 Oct): this branch builds the `Kick` target on
Ubuntu 24.04 (GCC 13.3, CMake 3.28, Ninja) with no source changes, and runs to the Projects
screen. Two things to know there:

- On Linux the app uses the device's own paths (`kick_paths.cpp`): `/app/assets`,
  `/app/demos/`, `/app/templates/`, `/data/sounds` and `/home/Kick/{projects,logs,prefs}`.
  Without them it crashes on start in `RecentProject::visibilityChanged()`, which indexes
  `recentItems` even when it is empty. Symlinking `Kick/assets`, the demos and the templates
  into `/app` works on a desktop. This branch now guards that line, so the empty list should
  no longer crash it (not yet re-tested without the folders); the demos and templates still
  need the paths to appear.
- The `KICK_SNAPSHOT` hook below is compiled out on Linux; Xvfb and ImageMagick's `import`
  take a headless screenshot instead.

## Not done yet

- **Quitting** ends in a crash after the window closes, in `Kick` and `KT_KickEM`. Static
  `MTAudioInterface` is destroyed after JUCE's MIDI device-list singleton
  (`~MidiDeviceListConnection`). macOS may then reopen the app on its own.
- Sample preview, the looper and the sampler have been tried by ear in `KT_KickEM`; the drum
  filter and most other audio paths have not.
- Nothing has been built for the device itself (aarch64 Buildroot).

`Kick/apps/kick/Main.cpp` also gains a desktop-only debugging aid: set `KICK_SNAPSHOT` to a
PNG path and the app saves its window there after a few seconds and quits.
