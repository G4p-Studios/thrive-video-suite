// SPDX-License-Identifier: MIT
// Thrive Video Suite – Priority-queued announcement system

#pragma once

#include <QObject>
#include <QString>
#include <QQueue>
#include <QTimer>

namespace Thrive {

/// Manages a queue of screen reader announcements with priority levels.
/// High-priority messages (navigation feedback) interrupt current speech;
/// low-priority messages (status updates) are queued and spoken in order.
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
    struct Message {
        QString  text;
        Priority priority;
    };

    bool m_enabled = true;
    QQueue<Message> m_queue;
    QTimer m_processTimer;
};

} // namespace Thrive
