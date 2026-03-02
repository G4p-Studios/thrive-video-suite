# Architecture

This document describes the high-level architecture of Thrive Video Suite.
If you want to contribute, this is the best place to start.

## Design Principles

1. **Accessibility first** — every feature must be usable without sight.
   Screen reader output, audio cues, and keyboard navigation are not
   afterthoughts; they are primary design requirements.

2. **Layered architecture** — the codebase is split into four layers that
   form a strict dependency chain. Lower layers never depend on higher ones.

3. **Undo everywhere** — every user-facing mutation is wrapped in a
   `QUndoCommand` subclass so that Ctrl+Z works reliably across the board.

4. **Internationalisation ready** — every user-visible string uses `tr()`.

## Layer Diagram

```
┌───────────────────────────────────────────────────────┐
│                     src/app/                          │
│  main.cpp · MainWindow · ShortcutManager · Wizard     │
├───────────────────────────────────────────────────────┤
│                      src/ui/                          │
│  TimelineWidget · TransportBar · MediaBrowser · …     │
├──────────────────┬────────────────────────────────────┤
│ src/engine/      │        src/accessibility/          │
│ MltEngine        │  ScreenReader (Prism)              │
│ PlaybackCtrl     │  Announcer (priority queue)        │
│ RenderEngine     │  AccessibleTimeline (bridge)       │
│ EffectCatalog    │  AudioCueManager (QSoundEffect)    │
├──────────────────┴────────────────────────────────────┤
│                     src/core/                         │
│  Timeline · Track · Clip · Effect · Transition ·      │
│  Marker · TimeCode · Project · Commands ·             │
│  ProjectSerializer                                    │
└───────────────────────────────────────────────────────┘
```

## Layers in Detail

### `src/core/` — Data Model

Pure data classes with no UI or engine dependencies.  Links only to
`Qt6::Core` and `QuaZip`.

| Class | Responsibility |
|---|---|
| `TimeCode` | Frame-based time with `toString()` / `toSpokenString()` |
| `Clip` | A segment of source media on a track (in/out points, effects, transitions) |
| `Track` | An ordered list of clips; either Video or Audio type |
| `Timeline` | The top-level model: tracks + markers + playhead + navigation state |
| `Effect` | An MLT filter instance with typed parameters |
| `Transition` | A transition between two adjacent clips |
| `Marker` | A named navigation landmark (position + comment) |
| `Project` | Owns the timeline and project-level settings (FPS, resolution, scrub audio) |
| `ProjectSerializer` | Reads/writes `.tvs` ZIP containers (MLT XML + `metadata.json`) |
| `commands.h` | 11 `QUndoCommand` subclasses for every model mutation |

**Key design choice:** `removeClip`, `removeTrack`, `removeMarker`, and
`removeEffect` do **not** call `deleteLater()` on the removed object.
This is intentional — the undo commands hold raw pointers and re-insert
objects on undo.  Lifetime is managed by the undo stack or by QObject
parent ownership.

### `src/engine/` — Video Engine

Wraps [MLT Framework 7](https://www.mltframework.org/) and exposes a
Qt-idiomatic API.  Links to `thrive-core`, MLT, and MLT++.

| Class | Responsibility |
|---|---|
| `MltEngine` | Initialises MLT, manages dual profiles (composition + preview) |
| `PlaybackController` | Play/pause/stop/seek, J-K-L speed control, scrub audio |
| `RenderEngine` | Background rendering with progress signals |
| `EffectCatalog` | Scans MLT for available filters, builds a searchable catalog |

**Dual-profile architecture:** The composition profile is always at the
project's native resolution (e.g. 1920×1080).  A separate preview profile
renders at a lower resolution (default 640p) for performance.  Both
profiles share the same frame rate and colour space.

### `src/accessibility/` — Screen Reader & Audio Cues

Links to `thrive-core`, `Qt6::Multimedia`, and
[Prism](https://github.com/ethindp/prism).

| Class | Responsibility |
|---|---|
| `ScreenReader` | Singleton wrapper around Prism's C API |
| `Announcer` | Priority queue (High/Normal/Low) with a 50 ms drain timer |
| `AccessibleTimeline` | Bridges `Timeline` signals to `Announcer` messages |
| `AudioCueManager` | Plays short WAV tones via `QSoundEffect` for clip boundaries, gaps, track edges |

**Priority queue semantics:**
- **High** — interrupts current speech (navigation, errors)
- **Normal** — queued after current speech (clip details, confirmations)
- **Low** — spoken only when queue is empty (status bar updates)

### `src/ui/` — Qt Widgets

All widget code.  Compiled with C++20 to avoid header conflicts between
Qt 6 and C++23.  Links to all other libraries.

| Class | Responsibility |
|---|---|
| `TimelineWidget` | Custom widget hosting the timeline table; owns a `Timeline` instance |
| `AccessibleTimelineView` | `QAccessibleTableInterface` + `QAccessibleTableCellInterface` implementation |
| `TransportBar` | Play/pause/stop/seek buttons + timecode display |
| `MediaBrowser` | File system browser with accessible tree view |
| `PropertiesPanel` | Inspector for the currently selected clip/track |
| `EffectsBrowser` | Searchable effect list with spoken descriptions |
| `PreferencesDialog` | Tabbed dialog for project + app settings |
| `ShortcutEditor` | `QKeySequenceEdit`-based shortcut customisation with SR conflict detection |
| `PluginManager` | Install/remove third-party plugins with restart flow |

### `src/app/` — Application Shell

The entry point and top-level wiring.  Compiled with C++20.

| Class | Responsibility |
|---|---|
| `main.cpp` | Creates `QApplication`, `Project`, `MltEngine`, `Announcer`, `AudioCueManager`, and `MainWindow`.  Implements the `EXIT_RESTART` loop. |
| `MainWindow` | Assembles all panels as `QDockWidget`s, wires actions, menus, and shortcuts |
| `ShortcutManager` | Singleton that persists shortcuts in `QSettings` |
| `WelcomeWizard` | 7-page `QWizard` for first-run onboarding |
| `constants.h` | App version, settings keys, `EXIT_RESTART` value |

## Project File Format (`.tvs`)

A `.tvs` file is a **ZIP archive** containing:

```
my_project.tvs (ZIP)
├── project.mlt      ← MLT XML describing the timeline
└── metadata.json    ← Project settings (name, fps, resolution, etc.)
```

Handled by `ProjectSerializer` using QuaZip.

## Undo System

Every user-facing mutation goes through a `QUndoCommand` subclass pushed
onto a `QUndoStack` in `MainWindow`.  The 11 command classes are:

| Command | What It Does |
|---|---|
| `AddClipCommand` | Inserts a clip into a track |
| `RemoveClipCommand` | Removes a clip from a track |
| `MoveClipCommand` | Reorders a clip within a track |
| `TrimClipCommand` | Changes a clip's in or out point |
| `SplitClipCommand` | Splits a clip at a given frame |
| `AddTrackCommand` | Adds a track to the timeline |
| `RemoveTrackCommand` | Removes a track from the timeline |
| `AddEffectCommand` | Applies an effect to a clip |
| `RemoveEffectCommand` | Removes an effect from a clip |
| `AddMarkerCommand` | Adds a navigation marker |
| `RemoveMarkerCommand` | Removes a navigation marker |

## Testing

Tests live in `tests/` and use **Qt Test** (`QTest`).  Each test file
produces a separate executable.  Run them via:

```bash
cd build && ctest --output-on-failure
```

Current test coverage:

| Test | What It Covers |
|---|---|
| `test_timecode` | TimeCode arithmetic, formatting, parsing, comparison |
| `test_timeline` | Track/clip/marker navigation, playhead movement |
| `test_clip` | Clip properties, timing, effects, transitions, accessible summary |
| `test_track` | Track constructors, clip CRUD, track effects, accessible summary |
| `test_effect` | Effect properties, parameters, enable/disable, accessible summary |
| `test_marker` | Marker properties, signal emission, accessible summary |
| `test_project` | Project defaults, resolution, FPS, scrub audio, reset |
| `test_commands` | All 11 undo commands: redo, undo, re-redo, compound stacks |
| `test_announcer` | Announcer enable/disable, queue, timer drain |

## Accessibility Implementation Details

### Screen Reader Integration

`AccessibleTimelineView` implements two Qt accessibility interfaces:

- `QAccessibleTableInterface` — exposes the timeline as a table where
  tracks are rows and clips are cells
- `QAccessibleTableCellInterface` — provides per-cell text via
  `Clip::accessibleSummary()`

The accessibility factory is registered via a static initialiser in the
view's translation unit, matching on the class name
`"Thrive::AccessibleTimelineView"`.

### Keyboard Navigation

All navigation emits through `Timeline` signals → `AccessibleTimeline`
bridge → `Announcer` queue → `ScreenReader` output.  This ensures every
navigation action produces spoken feedback within one event-loop cycle.

### Screen Reader Shortcut Conflict Detection

`ShortcutEditor` maintains a blocklist of modifier prefixes that conflict
with common screen readers:

- `Insert+` / `Ins+` — JAWS, NVDA
- `CapsLock+` — NVDA (laptop layout)
- `ScrollLock+` — various

When a user assigns a shortcut containing one of these prefixes, a
visible and announced warning is displayed.

## Build Configuration Notes

- **C++23** is the global standard.  Qt-facing targets (`src/ui/`,
  `src/app/`) use **C++20** via `set_target_properties(... CXX_STANDARD 20)`
  to avoid known conflicts between Qt 6 headers and C++23 library headers.
  This is ABI-compatible within the same MSVC toolset.

- **`QT_NO_CAST_FROM_ASCII`** is defined globally to prevent implicit
  `const char*` → `QString` conversions, enforcing explicit
  `QStringLiteral` or `QLatin1String` usage.

- **Prism** and **QuaZip** are fetched at configure time via
  `FetchContent`.  No manual installation needed.

- **MLT Framework** must be installed separately and discoverable via
  `pkg-config`.
