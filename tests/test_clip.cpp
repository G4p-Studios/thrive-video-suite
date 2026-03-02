// SPDX-License-Identifier: MIT
// Thrive Video Suite – Clip unit tests

#include <QTest>
#include <QSignalSpy>
#include "../src/core/clip.h"
#include "../src/core/effect.h"
#include "../src/core/transition.h"
#include "../src/core/timecode.h"

using namespace Thrive;

class TestClip : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_clip = new Clip(this);
    }

    void cleanup()
    {
        delete m_clip;
        m_clip = nullptr;
    }

    // ── identity ────────────────────────────────────────────────────

    void defaultConstructor()
    {
        QVERIFY(m_clip->name().isEmpty());
        QVERIFY(m_clip->description().isEmpty());
        QVERIFY(m_clip->sourcePath().isEmpty());
        QCOMPARE(m_clip->inPoint().frame(), 0);
        QCOMPARE(m_clip->outPoint().frame(), 0);
    }

    void namedConstructor()
    {
        TimeCode in(10, 25.0), out(100, 25.0);
        Clip clip(QStringLiteral("Intro.mp4"),
                  QStringLiteral("/media/intro.mp4"),
                  in, out, this);

        QCOMPARE(clip.name(), QStringLiteral("Intro.mp4"));
        QCOMPARE(clip.sourcePath(), QStringLiteral("/media/intro.mp4"));
        QCOMPARE(clip.inPoint().frame(), 10);
        QCOMPARE(clip.outPoint().frame(), 100);
    }

    void setNameEmitsSignal()
    {
        QSignalSpy spy(m_clip, &Clip::nameChanged);
        m_clip->setName(QStringLiteral("Test"));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toString(), QStringLiteral("Test"));
    }

    void setNameNoDuplicateSignal()
    {
        m_clip->setName(QStringLiteral("Test"));
        QSignalSpy spy(m_clip, &Clip::nameChanged);
        m_clip->setName(QStringLiteral("Test")); // same value
        QCOMPARE(spy.count(), 0);
    }

    void setDescription()
    {
        m_clip->setDescription(QStringLiteral("A test clip"));
        QCOMPARE(m_clip->description(), QStringLiteral("A test clip"));
    }

    // ── timing ──────────────────────────────────────────────────────

    void duration()
    {
        m_clip->setInPoint(TimeCode(10, 25.0));
        m_clip->setOutPoint(TimeCode(110, 25.0));
        QCOMPARE(m_clip->duration().frame(), 100);
    }

    void setTimingEmitsSignal()
    {
        QSignalSpy spy(m_clip, &Clip::timingChanged);
        m_clip->setInPoint(TimeCode(5, 25.0));
        QCOMPARE(spy.count(), 1);
        m_clip->setOutPoint(TimeCode(50, 25.0));
        QCOMPARE(spy.count(), 2);
        m_clip->setTimelinePosition(TimeCode(100, 25.0));
        QCOMPARE(spy.count(), 3);
    }

    void timelinePosition()
    {
        m_clip->setTimelinePosition(TimeCode(500, 25.0));
        QCOMPARE(m_clip->timelinePosition().frame(), 500);
    }

    // ── effects ─────────────────────────────────────────────────────

    void addAndRemoveEffect()
    {
        auto *fx = new Effect(QStringLiteral("brightness"),
                              QStringLiteral("Brightness"),
                              QStringLiteral("Adjust brightness"));
        QSignalSpy spy(m_clip, &Clip::effectsChanged);

        m_clip->addEffect(fx);
        QCOMPARE(m_clip->effects().size(), 1);
        QCOMPARE(spy.count(), 1);

        m_clip->removeEffect(0);
        QCOMPARE(m_clip->effects().size(), 0);
        QCOMPARE(spy.count(), 2);
    }

    void moveEffect()
    {
        auto *fx1 = new Effect(QStringLiteral("a"), QStringLiteral("A"),
                               QStringLiteral(""));
        auto *fx2 = new Effect(QStringLiteral("b"), QStringLiteral("B"),
                               QStringLiteral(""));
        m_clip->addEffect(fx1);
        m_clip->addEffect(fx2);

        m_clip->moveEffect(0, 1);
        QCOMPARE(m_clip->effects().at(0), fx2);
        QCOMPARE(m_clip->effects().at(1), fx1);
    }

    void removeEffectOutOfRange()
    {
        QSignalSpy spy(m_clip, &Clip::effectsChanged);
        m_clip->removeEffect(99);   // should not crash
        QCOMPARE(spy.count(), 0);
    }

    // ── transitions ─────────────────────────────────────────────────

    void transitions()
    {
        auto *tIn  = new Transition(QStringLiteral("dissolve"),
                                    QStringLiteral("Dissolve"),
                                    QStringLiteral(""),
                                    TimeCode(25, 25.0), this);
        auto *tOut = new Transition(QStringLiteral("wipe"),
                                    QStringLiteral("Wipe"),
                                    QStringLiteral(""),
                                    TimeCode(12, 25.0), this);

        m_clip->setInTransition(tIn);
        m_clip->setOutTransition(tOut);

        QCOMPARE(m_clip->inTransition(), tIn);
        QCOMPARE(m_clip->outTransition(), tOut);
    }

    // ── accessibility ───────────────────────────────────────────────

    void accessibleSummary()
    {
        m_clip->setName(QStringLiteral("Interview.mp4"));
        m_clip->setInPoint(TimeCode(0, 25.0));
        m_clip->setOutPoint(TimeCode(250, 25.0));

        const QString s = m_clip->accessibleSummary();
        QVERIFY(s.contains(QStringLiteral("Interview.mp4")));
        QVERIFY(s.contains(QStringLiteral("Clip")));
    }

    void accessibleSummaryWithEffects()
    {
        m_clip->setName(QStringLiteral("Scene.mp4"));
        m_clip->setInPoint(TimeCode(0, 25.0));
        m_clip->setOutPoint(TimeCode(50, 25.0));

        auto *fx = new Effect(QStringLiteral("blur"),
                              QStringLiteral("Blur"),
                              QStringLiteral("Blurs"));
        m_clip->addEffect(fx);

        const QString s = m_clip->accessibleSummary();
        QVERIFY(s.contains(QStringLiteral("effect")));
    }

private:
    Clip *m_clip = nullptr;
};

QTEST_MAIN(TestClip)
#include "test_clip.moc"
