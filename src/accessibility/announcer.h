// SPDX-License-Identifier: MIT
// Thrive Video Suite – Priority-queued announcement system

#pragma once

#include <QObject>
#include <QString>
#include <QQueue>
#include <QTimer>

class QWidget;

namespace Thrive {

/// Manages a queue of screen reader announcements with priority levels.
/// Uses Qt's native QAccessibleAnnouncementEvent (Qt 6.8+) to send
/// notifications through the UIA bridge that NVDA monitors directly.
/// Prism is used as a fallback for braille output.
class Announcer : public QObject
{
    Q_OBJECT

public:
    enum class Priority {
        High,   ///< Interrupts current speech (e.g. navigation, errors)
        Normal, ///< Queued after current speech (e.g. clip details)
        Low     ///< Spoken only when queue is empty (e.g. status bar)
    };
    Q_ENUM(Priority)

    explicit Announcer(QObject *parent = nullptr);

    /// Set the widget used as the source for accessibility events.
    /// Must be called before announce() will work. Typically the main window.
    void setTarget(QWidget *target) { m_target = target; }

    /// Queue a message for announcement.
    void announce(const QString &text, Priority priority = Priority::Normal);

    /// Clear all pending messages.
    void clearQueue();

    /// Whether announcements are enabled (user preference).
    [[nodiscard]] bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);

private slots:
    void processQueue();

private:
    void sendAccessibleAnnouncement(const QString &text, bool assertive);

    struct Message {
        QString  text;
        Priority priority;
    };

    bool m_enabled = true;
    QWidget *m_target = nullptr;
    QQueue<Message> m_queue;
    QTimer m_processTimer;
};

} // namespace Thrive
