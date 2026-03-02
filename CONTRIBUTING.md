# Contributing to Thrive Video Suite

Thank you for your interest in contributing!  Thrive Video Suite exists to make video
editing accessible to blind and visually impaired users, and every
contribution moves that mission forward.

## Getting Started

1. **Read [ARCHITECTURE.md](ARCHITECTURE.md)** — it explains the layer
   diagram, design principles, and key classes.

2. **Set up the build** — see [README.md](README.md) for prerequisites
   and build instructions.

3. **Run the tests** — `cd build && ctest --output-on-failure`.  All
   tests must pass before you submit a PR.

## Development Guidelines

### Code Style

- **C++23** for `src/core/` and `src/engine/`.
- **C++20** for `src/ui/` and `src/app/` (Qt header compatibility).
- Use `QStringLiteral("…")` or `QLatin1String("…")` instead of raw
  string literals — `QT_NO_CAST_FROM_ASCII` is enabled.
- Follow Qt naming conventions: `camelCase` for methods and variables,
  `PascalCase` for classes, `m_` prefix for members.
- Every class lives in its own `.h` / `.cpp` pair.
- Keep headers minimal — prefer forward declarations over includes.

### Accessibility Requirements

**Every feature must be accessible.**  This is not optional.

- All user-visible text must use `tr()` for internationalisation.
- Every widget must set `setAccessibleName()` and, where appropriate,
  `setAccessibleDescription()`.
- Navigation actions must emit a `Timeline` signal that flows through
  `AccessibleTimeline` → `Announcer` → `ScreenReader`.
- Test your changes with a screen reader (NVDA is free and recommended).
- If you add a new keyboard shortcut, check it doesn't conflict with
  screen reader modifiers (Insert, CapsLock, ScrollLock).

### Undo Support

Every model mutation must be wrapped in a `QUndoCommand` subclass.  See
`src/core/commands.h` for the existing commands.  The pattern is:

1. Define a command class in `commands.h` / `commands.cpp`.
2. Push it onto the `QUndoStack` in `MainWindow` (or wherever the action
   is triggered).
3. Write a test in `tests/test_commands.cpp` that verifies redo, undo,
   and re-redo.

**Important:** The `remove*` methods on `Track`, `Clip`, `Timeline` do
**not** call `deleteLater()`.  The undo commands hold raw pointers and
re-insert objects on undo.  If you add a new remove operation, follow
the same pattern.

### Testing

- Every new class should have a corresponding test file in `tests/`.
- Use the `thrive_add_test()` macro in `tests/CMakeLists.txt` to register it.
- Test both the happy path and edge cases (out-of-range indices, empty
  states, signal emission, no-crash on invalid input).
- Tests that need the accessibility library should add `thrive-accessibility`
  as an extra link library.

### Commit Messages

Use clear, imperative-mood commit messages:

```
Add SplitClipCommand with undo support

Splits a clip at a given frame position. The new clip inherits the
source path and effects of the original. Undo restores the original
out point and removes the split fragment.
```

### Pull Requests

- One feature or fix per PR.
- Reference any related issues in the PR description.
- Ensure all tests pass and no new compiler warnings are introduced.
- If you add a new source file, add it to the appropriate `CMakeLists.txt`.
- If you add a new test file, add it to `tests/CMakeLists.txt`.

## Project Structure

```
src/
├── core/           Data model (no UI, no engine)
├── engine/         MLT Framework wrapper
├── accessibility/  Screen reader, announcer, audio cues
├── ui/             Qt widgets
└── app/            Entry point, main window, shortcuts, wizard

tests/              Qt Test unit tests
resources/          Audio cue WAV files, .qrc manifest
docs/               (future) User-facing documentation
```

## Areas Where Help Is Needed

- **Media import pipeline** — wiring drag-and-drop / file dialog import
  to clip creation on the timeline.
- **MLT tractor building** — constructing the MLT tractor/multitrack
  from the `Timeline` model for playback and rendering.
- **Cut / copy / paste** — clipboard operations for clips and tracks.
- **Audio waveform display** — an accessible representation of audio
  levels on the timeline.
- **Internationalisation** — translating UI strings to additional
  languages.
- **Documentation** — user-facing tutorials and screen reader workflow
  guides.

## Code of Conduct

Be kind, be respectful, and remember that accessibility is a human right.
We welcome contributors of all backgrounds and experience levels.

## License

By contributing, you agree that your contributions will be licensed under
the MIT License — see [LICENSE](LICENSE) for details.
