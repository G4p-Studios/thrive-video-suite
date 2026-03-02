// SPDX-License-Identifier: MIT
// Thrive Video Suite – Announcer unit tests

#include <QTest>
#include "../src/accessibility/announcer.h"

using namespace Thrive;

class TestAnnouncer : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_announcer = new Announcer(this);
    }

    void cleanup()
    {
        delete m_announcer;
        m_announcer = nullptr;
    }

    // ── basic functionality ─────────────────────────────────────────

    void defaultEnabled()
    {
        QVERIFY(m_announcer->isEnabled());
    }

    void disableEnable()
    {
        m_announcer->setEnabled(false);
        QVERIFY(!m_announcer->isEnabled());

        m_announcer->setEnabled(true);
        QVERIFY(m_announcer->isEnabled());
    }

    void announceDoesNotCrash()
    {
        // Without a real screen reader, just ensure no crash
        m_announcer->announce(QStringLiteral("Test message"),
                              Announcer::Priority::High);
        m_announcer->announce(QStringLiteral("Normal message"),
                              Announcer::Priority::Normal);
        m_announcer->announce(QStringLiteral("Low message"),
                              Announcer::Priority::Low);
    }

    void announceWhileDisabled()
    {
        m_announcer->setEnabled(false);
        m_announcer->announce(QStringLiteral("Should be ignored"),
                              Announcer::Priority::High);
        // No crash, message silently discarded
    }

    void clearQueue()
    {
        m_announcer->announce(QStringLiteral("A"), Announcer::Priority::Low);
        m_announcer->announce(QStringLiteral("B"), Announcer::Priority::Low);
        m_announcer->clearQueue();
        // No crash; queue is empty now
    }

    void processQueueViaTimer()
    {
        // Queue a message and let the timer fire
        m_announcer->announce(QStringLiteral("Queued"),
                              Announcer::Priority::Normal);

        // Process events to let QTimer fire
        QTest::qWait(100);  // 50ms timer + headroom
        // If this completes without crash, queue was processed
    }

private:
    Announcer *m_announcer = nullptr;
};

QTEST_MAIN(TestAnnouncer)
#include "test_announcer.moc"
