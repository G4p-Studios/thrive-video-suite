// SPDX-License-Identifier: MIT
// Thrive Video Suite – Screen reader abstraction (Prism wrapper)

#pragma once

#include <QObject>
#include <QString>

namespace Thrive {

/// Singleton wrapper around the Prism screen reader library.
/// Provides speak/braille output through whatever screen reader is active.
class ScreenReader : public QObject
{
    Q_OBJECT

public:
    /// Get the singleton instance.
    static ScreenReader &instance();

    /// Initialise Prism. Call once at application startup.
    bool initialize();

    /// Shut down Prism. Called automatically on destruction.
    void shutdown();

    [[nodiscard]] bool isInitialized() const { return m_initialized; }

    /// Speak text through the active screen reader.
    /// @param interrupt  If true, interrupts any current speech first.
    void speak(const QString &text, bool interrupt = false);

    /// Output text to a braille display (if available).
    void braille(const QString &text);

    /// Combined speak + braille output.
    void output(const QString &text, bool interrupt = false);

    /// Stop all current speech.
    void silence();

    /// Check whether any screen reader is currently active.
    [[nodiscard]] bool isScreenReaderActive() const;

    /// Get the name of the detected screen reader (e.g. "NVDA", "JAWS").
    [[nodiscard]] QString detectedScreenReader() const;

private:
    ScreenReader();
    ~ScreenReader() override;
    ScreenReader(const ScreenReader &) = delete;
    ScreenReader &operator=(const ScreenReader &) = delete;

    bool  m_initialized = false;
    void *m_context = nullptr; // PrismContext*
    void *m_backend = nullptr; // PrismBackend*
};

} // namespace Thrive
