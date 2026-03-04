// SPDX-License-Identifier: MIT
// Thrive Video Suite – Announcer implementation

#include "announcer.h"
#include "screenreader.h"

#include <QAccessible>
#include <QAccessibleAnnouncementEvent>
#include <QWidget>
#include <QDebug>

namespace Thrive {

Announcer::Announcer(QObject *parent)
    : QObject(parent)
{
    m_processTimer.setInterval(50);
    m_processTimer.setSingleShot(true);
    connect(&m_processTimer, &QTimer::timeout, this, &Announcer::processQueue);
}

void Announcer::announce(const QString &text, Priority priority)
{
    if (!m_enabled || text.isEmpty())
        return;

    // High priority: speak immediately, interrupting everything
    if (priority == Priority::High) {
        m_queue.clear();
        sendAccessibleAnnouncement(text, /*assertive=*/true);
        return;
    }

    m_queue.enqueue({ text, priority });

    if (!m_processTimer.isActive())
        m_processTimer.start();
}

void Announcer::clearQueue()
{
    m_queue.clear();
}

void Announcer::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!enabled) {
        clearQueue();
        ScreenReader::instance().silence();
    }
}

void Announcer::processQueue()
{
    if (m_queue.isEmpty())
        return;

    // Process one message per tick
    const auto msg = m_queue.dequeue();
    sendAccessibleAnnouncement(msg.text, /*assertive=*/false);

    // Schedule next if more messages remain
    if (!m_queue.isEmpty())
        m_processTimer.start();
}

void Announcer::sendAccessibleAnnouncement(const QString &text,
                                           bool assertive)
{
    // Primary path: Qt's native accessibility announcement event.
    // This sends a UIA Notification through the same bridge NVDA monitors,
    // so it is far more reliable than Prism's separate NVDA controller.
    if (m_target) {
        QAccessibleAnnouncementEvent ev(m_target, text);
        ev.setPoliteness(assertive
                             ? QAccessible::AnnouncementPoliteness::Assertive
                             : QAccessible::AnnouncementPoliteness::Polite);
        QAccessible::updateAccessibility(&ev);
    }

    // Also send through Prism for braille display output
    ScreenReader::instance().braille(text);
}

} // namespace Thrive
