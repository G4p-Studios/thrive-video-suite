// SPDX-License-Identifier: MIT
// Thrive Video Suite – Announcer implementation

#include "announcer.h"
#include "screenreader.h"

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
        ScreenReader::instance().output(text, /*interrupt=*/true);
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
    ScreenReader::instance().output(msg.text, /*interrupt=*/false);

    // Schedule next if more messages remain
    if (!m_queue.isEmpty())
        m_processTimer.start();
}

} // namespace Thrive
