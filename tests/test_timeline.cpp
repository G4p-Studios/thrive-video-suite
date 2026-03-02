// SPDX-License-Identifier: MIT
// Thrive Video Suite – Timeline unit tests

#include <QTest>
#include "../src/core/timeline.h"
#include "../src/core/track.h"
#include "../src/core/clip.h"
#include "../src/core/marker.h"
#include "../src/core/timecode.h"

using namespace Thrive;

class TestTimeline : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_timeline = new Timeline(this);
    }

    void cleanup()
    {
        delete m_timeline;
        m_timeline = nullptr;
    }

    // ── track management ────────────────────────────────────────────

    void addTrack()
    {
        auto *video = new Track(Track::Type::Video, m_timeline);
        video->setName(QStringLiteral("Video 1"));
        m_timeline->addTrack(video);
        QCOMPARE(m_timeline->tracks().size(), 1);
    }

    void navigateTracks()
    {
        auto *v1 = new Track(Track::Type::Video, m_timeline);
        auto *a1 = new Track(Track::Type::Audio, m_timeline);
        m_timeline->addTrack(v1);
        m_timeline->addTrack(a1);

        QCOMPARE(m_timeline->currentTrackIndex(), 0);

        m_timeline->navigateNextTrack();
        QCOMPARE(m_timeline->currentTrackIndex(), 1);

        m_timeline->navigateNextTrack();
        // Should clamp at the last track
        QCOMPARE(m_timeline->currentTrackIndex(), 1);

        m_timeline->navigatePreviousTrack();
        QCOMPARE(m_timeline->currentTrackIndex(), 0);

        m_timeline->navigatePreviousTrack();
        // Should clamp at the first track
        QCOMPARE(m_timeline->currentTrackIndex(), 0);
    }

    // ── clip navigation ─────────────────────────────────────────────

    void navigateClips()
    {
        auto *trk = new Track(Track::Type::Video, m_timeline);
        m_timeline->addTrack(trk);

        auto *c1 = new Clip(trk);
        c1->setName(QStringLiteral("Clip A"));
        trk->addClip(c1);

        auto *c2 = new Clip(trk);
        c2->setName(QStringLiteral("Clip B"));
        trk->addClip(c2);

        QCOMPARE(m_timeline->currentClipIndex(), 0);

        m_timeline->navigateNextClip();
        QCOMPARE(m_timeline->currentClipIndex(), 1);

        m_timeline->navigateNextClip();
        QCOMPARE(m_timeline->currentClipIndex(), 1); // clamped

        m_timeline->navigatePreviousClip();
        QCOMPARE(m_timeline->currentClipIndex(), 0);
    }

    // ── markers ─────────────────────────────────────────────────────

    void addMarker()
    {
        auto *m = new Marker(m_timeline);
        m->setName(QStringLiteral("Chapter 1"));
        m->setPosition(TimeCode(30, 30.0));
        m_timeline->addMarker(m);
        QCOMPARE(m_timeline->markers().size(), 1);
    }

    void navigateMarkers()
    {
        auto *m1 = new Marker(m_timeline);
        m1->setPosition(TimeCode(0, 30.0));
        auto *m2 = new Marker(m_timeline);
        m2->setPosition(TimeCode(60, 30.0));
        auto *m3 = new Marker(m_timeline);
        m3->setPosition(TimeCode(120, 30.0));

        m_timeline->addMarker(m1);
        m_timeline->addMarker(m2);
        m_timeline->addMarker(m3);

        m_timeline->setPlayheadPosition(TimeCode(0, 30.0));
        m_timeline->navigateNextMarker();
        QCOMPARE(m_timeline->playheadPosition().frame(), 60);

        m_timeline->navigateNextMarker();
        QCOMPARE(m_timeline->playheadPosition().frame(), 120);

        m_timeline->navigatePreviousMarker();
        QCOMPARE(m_timeline->playheadPosition().frame(), 60);
    }

    // ── accessible summary ──────────────────────────────────────────

    void clipAccessibleSummary()
    {
        auto *trk = new Track(Track::Type::Video, m_timeline);
        m_timeline->addTrack(trk);

        auto *clip = new Clip(trk);
        clip->setName(QStringLiteral("Intro.mp4"));
        clip->setInPoint(TimeCode(0, 30.0));
        clip->setOutPoint(TimeCode(150, 30.0));
        trk->addClip(clip);

        const QString summary = clip->accessibleSummary();
        QVERIFY(!summary.isEmpty());
        QVERIFY(summary.contains(QStringLiteral("Intro.mp4")));
    }

private:
    Timeline *m_timeline = nullptr;
};

QTEST_MAIN(TestTimeline)
#include "test_timeline.moc"
