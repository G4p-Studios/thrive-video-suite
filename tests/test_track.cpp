// SPDX-License-Identifier: MIT
// Thrive Video Suite – Track unit tests

#include <QTest>
#include <QSignalSpy>
#include "../src/core/track.h"
#include "../src/core/clip.h"
#include "../src/core/effect.h"
#include "../src/core/timecode.h"

using namespace Thrive;

class TestTrack : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_track = new Track(Track::Type::Video, this);
        m_track->setName(QStringLiteral("Video 1"));
    }

    void cleanup()
    {
        delete m_track;
        m_track = nullptr;
    }

    // ── constructors ────────────────────────────────────────────────

    void defaultConstructor()
    {
        Track t;
        QCOMPARE(t.type(), Track::Type::Video);
        QVERIFY(t.name().isEmpty());
        QVERIFY(!t.isMuted());
        QVERIFY(!t.isLocked());
    }

    void typeOnlyConstructor()
    {
        Track t(Track::Type::Audio);
        QCOMPARE(t.type(), Track::Type::Audio);
    }

    void namedConstructor()
    {
        Track t(QStringLiteral("Audio 1"), Track::Type::Audio);
        QCOMPARE(t.name(), QStringLiteral("Audio 1"));
        QCOMPARE(t.type(), Track::Type::Audio);
    }

    // ── properties ──────────────────────────────────────────────────

    void setNameEmitsSignal()
    {
        QSignalSpy spy(m_track, &Track::nameChanged);
        m_track->setName(QStringLiteral("Renamed"));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(m_track->name(), QStringLiteral("Renamed"));
    }

    void setMutedEmitsSignal()
    {
        QSignalSpy spy(m_track, &Track::mutedChanged);
        m_track->setMuted(true);
        QCOMPARE(spy.count(), 1);
        QVERIFY(m_track->isMuted());

        m_track->setMuted(true); // same value
        QCOMPARE(spy.count(), 1); // no duplicate
    }

    void setLockedEmitsSignal()
    {
        QSignalSpy spy(m_track, &Track::lockedChanged);
        m_track->setLocked(true);
        QCOMPARE(spy.count(), 1);
        QVERIFY(m_track->isLocked());
    }

    // ── clip management ─────────────────────────────────────────────

    void addClip()
    {
        auto *clip = new Clip(QStringLiteral("A"), QStringLiteral("/a.mp4"),
                              TimeCode(0, 25.0), TimeCode(50, 25.0));
        QSignalSpy spy(m_track, &Track::clipsChanged);
        m_track->addClip(clip);

        QCOMPARE(m_track->clipCount(), 1);
        QCOMPARE(m_track->clipAt(0), clip);
        QCOMPARE(spy.count(), 1);
    }

    void insertClip()
    {
        auto *c1 = new Clip(QStringLiteral("C1"), {}, TimeCode(0, 25.0), TimeCode(10, 25.0));
        auto *c2 = new Clip(QStringLiteral("C2"), {}, TimeCode(10, 25.0), TimeCode(20, 25.0));
        auto *c3 = new Clip(QStringLiteral("C3"), {}, TimeCode(20, 25.0), TimeCode(30, 25.0));

        m_track->addClip(c1);
        m_track->addClip(c3);
        m_track->insertClip(1, c2);

        QCOMPARE(m_track->clipCount(), 3);
        QCOMPARE(m_track->clipAt(0)->name(), QStringLiteral("C1"));
        QCOMPARE(m_track->clipAt(1)->name(), QStringLiteral("C2"));
        QCOMPARE(m_track->clipAt(2)->name(), QStringLiteral("C3"));
    }

    void removeClip()
    {
        auto *clip = new Clip(QStringLiteral("A"), {}, TimeCode(), TimeCode(10, 25.0));
        m_track->addClip(clip);
        QCOMPARE(m_track->clipCount(), 1);

        m_track->removeClip(0);
        QCOMPARE(m_track->clipCount(), 0);
    }

    void removeClipOutOfRange()
    {
        QSignalSpy spy(m_track, &Track::clipsChanged);
        m_track->removeClip(5); // no crash
        QCOMPARE(spy.count(), 0);
    }

    void moveClip()
    {
        auto *c1 = new Clip(QStringLiteral("First"), {}, TimeCode(), TimeCode(10, 25.0));
        auto *c2 = new Clip(QStringLiteral("Second"), {}, TimeCode(), TimeCode(10, 25.0));
        m_track->addClip(c1);
        m_track->addClip(c2);

        m_track->moveClip(0, 1);
        QCOMPARE(m_track->clipAt(0)->name(), QStringLiteral("Second"));
        QCOMPARE(m_track->clipAt(1)->name(), QStringLiteral("First"));
    }

    void clipAtOutOfRange()
    {
        QVERIFY(m_track->clipAt(-1) == nullptr);
        QVERIFY(m_track->clipAt(999) == nullptr);
    }

    // ── track effects ───────────────────────────────────────────────

    void addTrackEffect()
    {
        auto *fx = new Effect(QStringLiteral("eq"),
                              QStringLiteral("Equalizer"),
                              QStringLiteral("Audio EQ"));
        QSignalSpy spy(m_track, &Track::trackEffectsChanged);

        m_track->addTrackEffect(fx);
        QCOMPARE(m_track->trackEffects().size(), 1);
        QCOMPARE(spy.count(), 1);
    }

    void removeTrackEffect()
    {
        auto *fx = new Effect(QStringLiteral("comp"),
                              QStringLiteral("Compressor"),
                              QStringLiteral(""));
        m_track->addTrackEffect(fx);

        m_track->removeTrackEffect(0);
        QCOMPARE(m_track->trackEffects().size(), 0);
    }

    // ── type string ─────────────────────────────────────────────────

    void typeString()
    {
        Track video(Track::Type::Video);
        Track audio(Track::Type::Audio);
        QCOMPARE(video.typeString(), QStringLiteral("Video"));
        QCOMPARE(audio.typeString(), QStringLiteral("Audio"));
    }

    // ── accessible summary ──────────────────────────────────────────

    void accessibleSummary()
    {
        const QString s = m_track->accessibleSummary();
        QVERIFY(s.contains(QStringLiteral("Video 1")));
        QVERIFY(s.contains(QStringLiteral("Video")));
        QVERIFY(s.contains(QStringLiteral("clip")));
    }

    void accessibleSummaryMuted()
    {
        m_track->setMuted(true);
        QVERIFY(m_track->accessibleSummary().contains(QStringLiteral("Muted")));
    }

    void accessibleSummaryLocked()
    {
        m_track->setLocked(true);
        QVERIFY(m_track->accessibleSummary().contains(QStringLiteral("Locked")));
    }

private:
    Track *m_track = nullptr;
};

QTEST_MAIN(TestTrack)
#include "test_track.moc"
