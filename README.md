# Thrive Video Suite

A **fully accessible non-linear video editor** built for blind and visually impaired users.

> "Blind people have been excluded from video editing for years. Thrive Video Suite aims to fix that."

## Features

- **Full screen reader integration** via [Prism](https://github.com/ethindp/prism) — works with NVDA, JAWS, Narrator, Orca, VoiceOver, and SAPI
- **Audio cues** — short tones at clip boundaries, gaps, and track edges
- **Keyboard-first design** — every function is reachable without a mouse
- **Reusable stack templates** — built-in and custom logo/intro presets with import/export via `.tstk`
- **Stack Manager** — create, import, export, and remove custom stack templates
- **Built-in logo presets** — Looney Tunes Intro, PBS 1971 Ident, PBS 1984 Ident
- **Optional stack soundtrack** — add logo audio with per-template default start offset on a dedicated audio track
- **J-K-L transport** — industry-standard playback speed control
- **Accessible timeline** — exposed as a table to screen readers (QAccessibleTableInterface)
- **Effect browser** — searchable with spoken descriptions for every effect
- **Customisable shortcuts** — with automatic screen reader key-conflict detection
- **Panel focus cycling** — F6 cycles timeline, transport, media, effects, and properties panels
- **Welcome wizard** — guided onboarding narrated page by page
- **Plugin support** — install/remove with app-restart activation flow
- **`.tvs` project format** — ZIP container (MLT XML + metadata.json)
- **i18n ready** — full `tr()` infrastructure from day one

## Stack Templates and Logo Workflows

Thrive Video Suite includes a full stack-template workflow designed for quickly building reusable logo and intro sequences.

- **Add Stack** (`Ctrl+Shift+B`) applies a selected template to the timeline.
- **Stack Manager** (Timeline menu, unassigned by default) manages custom templates.
- **Template format**: `.tstk` (JSON-backed stack presets).
- **Built-in templates**:
  - Looney Tunes Intro
  - PBS 1971 Ident
  - PBS 1984 Ident

When adding a stack, you can optionally include a soundtrack. If enabled, Thrive prompts for audio media and a start offset, then places the soundtrack on a dedicated **Logo Soundtrack** audio track.

## Technology

| Component | Technology |
|---|---|
| Language | C++23 (C++20 on Qt-facing targets) |
| UI Framework | Qt 6 Widgets (MSAA / IA2 accessibility) |
| Video Engine | MLT Framework 7 (MLT++ C++ wrapper) |
| Screen Reader | Prism v0.7.1 (C API, FetchContent) |
| ZIP I/O | QuaZip v1.5 (FetchContent) |
| Build System | CMake 3.21+ |
| Audio Cues | QSoundEffect (Qt Multimedia) |

## Prerequisites

- **Visual Studio 2022** — Community edition (free) with the "Desktop development with C++" workload
- **Qt 6.5+** — install via the [Qt online installer](https://www.qt.io/download-qt-installer-oss); select **MSVC 2022 64-bit** and **Qt Multimedia**
- **Git** — [git-scm.com](https://git-scm.com/download/win) (or `winget install Git.Git`)

Everything else (CMake, vcpkg, MLT Framework, pkg-config) is installed automatically by the build script.

## Building

```
build              # configure + build (installs deps on first run)
build test         # build + run all unit tests
build run          # build + launch the app
build clean        # delete the build directory
build reconfigure  # force a fresh cmake configure
build setup        # install dependencies only
```

That's it — one command. On a fresh clone the first `build` takes 10–20 minutes while vcpkg compiles packages and MLT is downloaded; subsequent builds are incremental and fast.

### First-time setup (step by step)

1. Install **Visual Studio 2022** with "Desktop development with C++"
2. Install **Qt 6** via the Qt online installer (select MSVC 2022 64-bit + Multimedia)
3. Clone this repo and run `build`:

```bash
git clone <repo-url>
cd thrive-video-suite
build
```

### Advanced / manual build

If you prefer to configure CMake yourself:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
  -DCMAKE_PREFIX_PATH="C:\Qt\6.11.0\msvc2022_64" ^
    -DCMAKE_TOOLCHAIN_FILE="C:\dev\thrive-deps\vcpkg\scripts\buildsystems\vcpkg.cmake"
cmake --build build
cd build && ctest --output-on-failure
```

## Testing

The project includes a comprehensive suite of unit tests using Qt Test.
Each test is a separate executable:

| Test | Coverage |
|---|---|
| `test_timecode` | Arithmetic, formatting, parsing, comparison operators |
| `test_timeline` | Track / clip / marker navigation, playhead |
| `test_clip` | Properties, timing, effects, transitions, accessible summary |
| `test_track` | Constructors, clip CRUD, track effects, accessible summary |
| `test_effect` | Properties, parameters, enable/disable, accessible summary |
| `test_marker` | Properties, signal emission, accessible summary |
| `test_project` | Defaults, resolution, FPS, scrub audio, reset |
| `test_stacktemplate` | Built-in template defaults, JSON round-trip, validation |
| `test_stackregistry` | Built-in availability, custom save/import/export/delete |
| `test_commands` | All 11 undo commands: redo → undo → re-redo + compound stacks |
| `test_announcer` | Enable/disable, queue, timer-drain |

```bash
cd build && ctest --output-on-failure
```

## Project Structure

```
src/
├── core/           # Data model (Timeline, Track, Clip, Effect, etc.)
├── engine/         # MLT engine wrapper, playback, render, effect catalog
├── accessibility/  # Screen reader (Prism), announcer, audio cues
├── ui/             # Qt widgets (timeline, transport, media browser, etc.)
└── app/            # Entry point, main window, shortcut manager, wizard

tests/              # Qt Test unit tests
resources/          # Audio cue WAV files, .qrc
```

## Key Shortcuts

The following tables list the default keyboard shortcuts by editing workflow.

### Transport and Playback

| Action | Shortcut |
|---|---|
| Play / Pause | Space |
| Rewind | J |
| Stop Transport | K |
| Fast Forward | L |
| Step Back 1 Frame | , (comma) |
| Step Forward 1 Frame | . (period) |
| Go to Timecode | Ctrl+G |

### Timeline Navigation

| Action | Shortcut |
|---|---|
| Previous / Next Track | Up / Down |
| Previous / Next Clip | Left / Right |
| Jump 5 Clips Back / Forward | Page Up / Page Down |
| Previous / Next Non-Empty Track | Ctrl+Page Up / Ctrl+Page Down |
| First / Last Clip on Current Track | Ctrl+Home / Ctrl+End |
| Timeline Start / End | Home / End |
| Next / Previous Marker | M / N |

### Timeline Editing

| Action | Shortcut |
|---|---|
| Split Clip at Playhead | S |
| Add Track | T |
| Remove Current Track | Shift+Delete |
| Add Stack | Ctrl+Shift+B |
| Stack Manager | Unassigned by default |
| Add Text Overlay Clip | Ctrl+Shift+T |
| Apply Motion Preset | Ctrl+Shift+P |
| Add Marker at Playhead | Shift+M |
| Remove Marker at Playhead | Ctrl+Shift+M |
| Add Transition | Shift+T |
| Move Clip to Track Above / Below | Shift+Up / Shift+Down |
| Move Track Up / Down | Alt+Up / Alt+Down |
| Nudge Clip Left / Right | Ctrl+Left / Ctrl+Right |
| Toggle Track Mute | Ctrl+M |
| Toggle Track Lock | Ctrl+Shift+L |
| Solo Current Track | Ctrl+Shift+O |
| Toggle Marker Jump Snap | Ctrl+Shift+J |

### Focus and Accessibility

| Action | Shortcut |
|---|---|
| Focus Timeline | Ctrl+1 |
| Focus Transport | Ctrl+2 |
| Focus Media Browser | Ctrl+I |
| Focus Effects Browser | Ctrl+E |
| Focus Properties Panel | Ctrl+P |
| Cycle Panel Focus | F6 |
| Announce Current Context | Ctrl+Shift+W |
| Cycle Context Verbosity (Short, Normal, Detailed) | Ctrl+Shift+V |
| Announce Keyboard Help | Ctrl+Shift+H |

### File and Edit

| Action | Shortcut |
|---|---|
| New Project | Ctrl+N |
| Open Project | Ctrl+O |
| Save | Ctrl+S |
| Save As | Ctrl+Shift+S |
| Export | Ctrl+Shift+E |
| Preferences | Ctrl+, |
| Quit | Ctrl+Q |
| Undo / Redo | Ctrl+Z / Ctrl+Y |
| Cut / Copy / Paste / Delete | Ctrl+X / Ctrl+C / Ctrl+V / Delete |
| Select All | Ctrl+A |

All shortcuts are customisable in **Preferences -> Shortcuts**. Context verbosity, marker jump snap, and Intro dry-run mode defaults are configurable in **Preferences -> General** and are persisted between sessions.

## Screen Reader Workflow Cheatsheet

This quick guide is for users who are new to editing video with a screen reader.

### 1) Start and Set Orientation

1. Open Preferences with `Ctrl+,` and choose your defaults in **General**.
2. Set **Context verbosity** to `Normal` or `Detailed` while learning.
3. Keep **Marker jump snap** enabled at first.
4. Set **Intro dry-run mode** to `Auto detect` unless you want to force visual or announcement-only dry-run behavior.
5. Use `Ctrl+Shift+W` any time to hear your current focus, playhead, and selection.
6. Use `Ctrl+Shift+H` to hear keyboard help at any point.

### 2) Build the Timeline Quickly

1. For logo and intro presets, use `Ctrl+Shift+B` (**Add Stack**) to apply a template.
2. When prompted, optionally attach soundtrack audio and choose its start time.
3. Import media from the Media Browser (`Ctrl+I` to focus it).
4. Move to Timeline with `Ctrl+1`.
5. Use `Up/Down` to choose track and `Left/Right` to choose clip.
6. Use `Page Up/Page Down` to jump 5 clips at a time.
7. Use `Ctrl+Page Up/Ctrl+Page Down` to jump between non-empty tracks.

### 3) Place and Refine Edits

1. Split at playhead with `S`.
2. Nudge clip timing with `Ctrl+Left/Ctrl+Right`.
3. Move clips between tracks with `Shift+Up/Shift+Down`.
4. Add marker at playhead with `Shift+M`.
5. Remove marker at playhead with `Ctrl+Shift+M`.

### 4) Navigate with Markers

1. Jump to next marker with `M` and previous marker with `N`.
2. Toggle marker snap with `Ctrl+Shift+J`.
3. With snap on: playhead moves to marker.
4. With snap off: marker position is announced without moving playhead.

### 5) Apply Effects and Check Context

1. Focus Effects Browser with `Ctrl+E`.
2. Focus Properties Panel with `Ctrl+P`.
3. Press `Ctrl+Shift+W` after each major edit step to verify location and selection.
4. Cycle verbosity with `Ctrl+Shift+V` if you need shorter or more detailed feedback.

### 6) Review and Export

1. Transport controls: `Space` (play/pause), `J` (rewind), `K` (stop), `L` (forward).
2. Frame-step with `,` and `.` for precise checks.
3. Go to exact timecode with `Ctrl+G`.
4. Export with `Ctrl+Shift+E`.

### Troubleshooting and Recovery

- If you lose track of where you are: press `Ctrl+Shift+W`.
- If navigation feels too noisy: cycle verbosity with `Ctrl+Shift+V`.
- If marker jumps are moving playhead unexpectedly: toggle snap with `Ctrl+Shift+J`.
- If key behavior feels wrong: open **Preferences -> Shortcuts** and check conflicts.

## Documentation

- [ARCHITECTURE.md](ARCHITECTURE.md) — layer diagram, design decisions,
  module-by-module breakdown, undo system, accessibility implementation
- [CONTRIBUTING.md](CONTRIBUTING.md) — code style, accessibility
  requirements, testing, PR guidelines, areas where help is needed

## License

MIT
