# Building the KICK app on a Mac

> **Provenance.** The stand-ins below come from the `macos-build` branch of
> [Eight4aWish/0700-AAM](https://github.com/Eight4aWish/0700-AAM/tree/macos-build) (AGPL-3.0, same
> project), merged here unchanged with its history. Changes made on top in this fork
> (NeonByteVoid/0700-AAM) are listed under [Additions in this fork](#additions-in-this-fork).

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

- **The simulator (`KT_KickEM`)** still fails: its panel code predates the current packet
  protocol (it uses `sig::Signal`, `mt::Kick::HwControlCommands` and button helpers that
  no longer exist). The `Kick` app does not need it.
- **Quitting** ends in an abort (exit 134) after the window closes.
- Audio, recording and the drum filter compile and are wired in, but have not been listened to.
- Nothing has been built for the device itself (aarch64 Buildroot).

`Kick/apps/kick/Main.cpp` also gains a desktop-only debugging aid: set `KICK_SNAPSHOT` to a
PNG path and the app saves its window there after a few seconds and quits.
