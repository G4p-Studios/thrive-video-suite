# Thrive Video Suite

A **fully accessible non-linear video editor** built for blind and visually impaired users.

> "Blind people have been excluded from video editing for years. Thrive Video Suite aims to fix that."

## Features

- **Full screen reader integration** via [Prism](https://github.com/ethindp/prism) — works with NVDA, JAWS, Narrator, Orca, VoiceOver, and SAPI
- **Audio cues** — short tones at clip boundaries, gaps, and track edges
- **Keyboard-first design** — every function is reachable without a mouse
- **J-K-L transport** — industry-standard playback speed control
- **Accessible timeline** — exposed as a table to screen readers (QAccessibleTableInterface)
- **Effect browser** — searchable with spoken descriptions for every effect
- **Customisable shortcuts** — with automatic screen reader key-conflict detection
- **Welcome wizard** — guided onboarding narrated page by page
- **Plugin support** — install/remove with app-restart activation flow
- **`.tvs` project format** — ZIP container (MLT XML + metadata.json)
- **i18n ready** — full `tr()` infrastructure from day one

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

- **Qt 6.5+** — install via the Qt online installer or your package manager
- **MLT Framework 7** — `pkg-config` must find `mlt-framework-7` and `mlt++-7`
- **CMake 3.21+**
- **A C++23-capable compiler** — MSVC 2022 17.6+, GCC 13+, or Clang 17+
- **pkg-config** — on Windows, install via `vcpkg install pkgconf` or use MSYS2

Prism and QuaZip are fetched automatically by CMake via `FetchContent`.

## Building

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# Run tests
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

| Action | Shortcut |
|---|---|
| Play / Pause | Space, K |
| Rewind | J |
| Fast Forward | L |
| Step Back 1 Frame | , (comma) |
| Step Forward 1 Frame | . (period) |
| Stop | S |
| Navigate Tracks | Up / Down |
| Navigate Clips | Left / Right |
| Next / Previous Marker | M / N |
| Import Media | Ctrl+I |
| Save | Ctrl+S |
| Export | Ctrl+Shift+E |
| Preferences | Ctrl+, |
| Undo / Redo | Ctrl+Z / Ctrl+Y |

All shortcuts are customisable in **Preferences → Shortcuts**. Key combinations that conflict with screen reader modifiers (Insert, Caps Lock, Scroll Lock) trigger a warning.

## Documentation

- [ARCHITECTURE.md](ARCHITECTURE.md) — layer diagram, design decisions,
  module-by-module breakdown, undo system, accessibility implementation
- [CONTRIBUTING.md](CONTRIBUTING.md) — code style, accessibility
  requirements, testing, PR guidelines, areas where help is needed

## License

MIT
