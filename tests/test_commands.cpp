// SPDX-License-Identifier: MIT
// Thrive Video Suite – Undo/Redo command unit tests

#include <QTest>
#include <QUndoStack>
#include "../src/core/commands.h"
#include "../src/core/timeline.h"
#include "../src/core/track.h"
#include "../src/core/clip.h"
#include "../src/core/effect.h"
#include "../src/core/marker.h"
#include "../src/core/timecode.h"

using namespace Thrive;

class TestCommands : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_stack = new QUndoStack(this);
        m_timeline = new Timeline(this);

        m_track = new Track(QStringLiteral("Video 1"), Track::Type::Video);
        m_timeline->addTrack(m_track);
    }

    void cleanup()
    {
        delete m_stack;
        m_stack = nullptr;
        delete m_timeline;
        m_timeline = nullptr;
        m_track = nullptr;
    }

    // ── AddClipCommand / RemoveClipCommand ──────────────────────────

    void addClipUndoRedo()
    {
        auto *clip = new Clip(QStringLiteral("A"), QStringLiteral("/a.mp4"),
                              TimeCode(0, 25.0), TimeCode(50, 25.0));

        m_stack->push(new AddClipCommand(m_track, clip));
        QCOMPARE(m_track->clipCount(), 1);
        QCOMPARE(m_track->clipAt(0)->name(), QStringLiteral("A"));

        m_stack->undo();
        QCOMPARE(m_track->clipCount(), 0);

        m_stack->redo();
        QCOMPARE(m_track->clipCount(), 1);
    }

    void addClipAtIndex()
    {
        auto *c1 = new Clip(QStringLiteral("C1"), {}, TimeCode(), TimeCode(10, 25.0));
        auto *c2 = new Clip(QStringLiteral("C2"), {}, TimeCode(), TimeCode(10, 25.0));
        auto *c3 = new Clip(QStringLiteral("C3"), {}, TimeCode(), TimeCode(10, 25.0));

        m_stack->push(new AddClipCommand(m_track, c1));
        m_stack->push(new AddClipCommand(m_track, c3));
        m_stack->push(new AddClipCommand(m_track, c2, 1)); // insert between

        QCOMPARE(m_track->clipCount(), 3);
        QCOMPARE(m_track->clipAt(1)->name(), QStringLiteral("C2"));
    }

    void removeClipUndoRedo()
    {
        auto *clip = new Clip(QStringLiteral("Victim"), {},
                              TimeCode(), TimeCode(25, 25.0));
        m_track->addClip(clip);
        QCOMPARE(m_track->clipCount(), 1);

        m_stack->push(new RemoveClipCommand(m_track, 0));
        QCOMPARE(m_track->clipCount(), 0);

        m_stack->undo();
        QCOMPARE(m_track->clipCount(), 1);
        QCOMPARE(m_track->clipAt(0)->name(), QStringLiteral("Victim"));

        m_stack->redo();
        QCOMPARE(m_track->clipCount(), 0);
    }

    // ── MoveClipCommand ─────────────────────────────────────────────

    void moveClipUndoRedo()
    {
        auto *c1 = new Clip(QStringLiteral("First"), {}, TimeCode(), TimeCode(10, 25.0));
        auto *c2 = new Clip(QStringLiteral("Second"), {}, TimeCode(), TimeCode(10, 25.0));
        m_track->addClip(c1);
        m_track->addClip(c2);

        m_stack->push(new MoveClipCommand(m_track, 0, 1));
        QCOMPARE(m_track->clipAt(0)->name(), QStringLiteral("Second"));

        m_stack->undo();
        QCOMPARE(m_track->clipAt(0)->name(), QStringLiteral("First"));
    }

    // ── TrimClipCommand ─────────────────────────────────────────────

    void trimInPointUndoRedo()
    {
        auto *clip = new Clip(QStringLiteral("Trim"), {},
                              TimeCode(10, 25.0), TimeCode(100, 25.0));
        m_track->addClip(clip);

        m_stack->push(new TrimClipCommand(clip, TrimClipCommand::Edge::In,
                                          TimeCode(20, 25.0)));
        QCOMPARE(clip->inPoint().frame(), 20);

        m_stack->undo();
        QCOMPARE(clip->inPoint().frame(), 10);
    }

    void trimOutPointUndoRedo()
    {
        auto *clip = new Clip(QStringLiteral("Trim"), {},
                              TimeCode(0, 25.0), TimeCode(100, 25.0));
        m_track->addClip(clip);

        m_stack->push(new TrimClipCommand(clip, TrimClipCommand::Edge::Out,
                                          TimeCode(80, 25.0)));
        QCOMPARE(clip->outPoint().frame(), 80);

        m_stack->undo();
        QCOMPARE(clip->outPoint().frame(), 100);
    }

    // ── SplitClipCommand ────────────────────────────────────────────

    void splitClipUndoRedo()
    {
        auto *clip = new Clip(QStringLiteral("Full"), {},
                              TimeCode(0, 25.0), TimeCode(100, 25.0));
        m_track->addClip(clip);

        m_stack->push(new SplitClipCommand(m_track, 0, TimeCode(50, 25.0)));

        // After split: original [0,50), new [50,100)
        QCOMPARE(m_track->clipCount(), 2);
        QCOMPARE(m_track->clipAt(0)->outPoint().frame(), 50);
        QCOMPARE(m_track->clipAt(1)->inPoint().frame(), 50);
        QCOMPARE(m_track->clipAt(1)->outPoint().frame(), 100);

        m_stack->undo();
        QCOMPARE(m_track->clipCount(), 1);
        QCOMPARE(m_track->clipAt(0)->outPoint().frame(), 100);
    }

    // ── AddTrackCommand / RemoveTrackCommand ────────────────────────

    void addTrackUndoRedo()
    {
        int initialCount = m_timeline->trackCount();
        auto *audio = new Track(QStringLiteral("Audio 1"), Track::Type::Audio);

        m_stack->push(new AddTrackCommand(m_timeline, audio));
        QCOMPARE(m_timeline->trackCount(), initialCount + 1);

        m_stack->undo();
        QCOMPARE(m_timeline->trackCount(), initialCount);

        m_stack->redo();
        QCOMPARE(m_timeline->trackCount(), initialCount + 1);
    }

    void removeTrackUndoRedo()
    {
        auto *extra = new Track(QStringLiteral("Extra"), Track::Type::Video);
        m_timeline->addTrack(extra);
        int count = m_timeline->trackCount();

        m_stack->push(new RemoveTrackCommand(m_timeline, count - 1));
        QCOMPARE(m_timeline->trackCount(), count - 1);

        m_stack->undo();
        QCOMPARE(m_timeline->trackCount(), count);
        QCOMPARE(m_timeline->trackAt(count - 1)->name(), QStringLiteral("Extra"));
    }

    // ── AddEffectCommand / RemoveEffectCommand ──────────────────────

    void addEffectUndoRedo()
    {
        auto *clip = new Clip(QStringLiteral("FxClip"), {},
                              TimeCode(), TimeCode(50, 25.0));
        m_track->addClip(clip);

        auto *fx = new Effect(QStringLiteral("blur"),
                              QStringLiteral("Blur"),
                              QStringLiteral("Gaussian blur"));

        m_stack->push(new AddEffectCommand(clip, fx));
        QCOMPARE(clip->effects().size(), 1);

        m_stack->undo();
        QCOMPARE(clip->effects().size(), 0);

        m_stack->redo();
        QCOMPARE(clip->effects().size(), 1);
    }

    void removeEffectUndoRedo()
    {
        auto *clip = new Clip(QStringLiteral("FxClip"), {},
                              TimeCode(), TimeCode(50, 25.0));
        m_track->addClip(clip);

        auto *fx = new Effect(QStringLiteral("sharpen"),
                              QStringLiteral("Sharpen"),
                              QStringLiteral(""));
        clip->addEffect(fx);

        m_stack->push(new RemoveEffectCommand(clip, 0));
        QCOMPARE(clip->effects().size(), 0);

        m_stack->undo();
        QCOMPARE(clip->effects().size(), 1);
        QCOMPARE(clip->effects().at(0)->serviceId(), QStringLiteral("sharpen"));
    }

    // ── AddMarkerCommand / RemoveMarkerCommand ──────────────────────

    void addMarkerUndoRedo()
    {
        m_stack->push(new AddMarkerCommand(m_timeline,
                                           QStringLiteral("Intro"),
                                           TimeCode(0, 25.0),
                                           QStringLiteral("Start")));
        QCOMPARE(m_timeline->markers().size(), 1);
        QCOMPARE(m_timeline->markers().first()->name(), QStringLiteral("Intro"));

        m_stack->undo();
        QCOMPARE(m_timeline->markers().size(), 0);

        m_stack->redo();
        QCOMPARE(m_timeline->markers().size(), 1);
    }

    void removeMarkerUndoRedo()
    {
        auto *marker = new Marker(QStringLiteral("Chapter 1"),
                                  TimeCode(250, 25.0),
                                  QStringLiteral("ch1"));
        m_timeline->addMarker(marker);
        QCOMPARE(m_timeline->markers().size(), 1);

        m_stack->push(new RemoveMarkerCommand(m_timeline, 0));
        QCOMPARE(m_timeline->markers().size(), 0);

        m_stack->undo();
        QCOMPARE(m_timeline->markers().size(), 1);
        QCOMPARE(m_timeline->markers().first()->name(), QStringLiteral("Chapter 1"));
    }

    // ── compound undo ───────────────────────────────────────────────

    void multipleUndos()
    {
        // Push several commands, then undo all
        auto *c1 = new Clip(QStringLiteral("One"), {}, TimeCode(), TimeCode(10, 25.0));
        auto *c2 = new Clip(QStringLiteral("Two"), {}, TimeCode(), TimeCode(10, 25.0));

        m_stack->push(new AddClipCommand(m_track, c1));
        m_stack->push(new AddClipCommand(m_track, c2));
        QCOMPARE(m_track->clipCount(), 2);

        m_stack->undo();
        QCOMPARE(m_track->clipCount(), 1);

        m_stack->undo();
        QCOMPARE(m_track->clipCount(), 0);

        m_stack->redo();
        m_stack->redo();
        QCOMPARE(m_track->clipCount(), 2);
    }

private:
    QUndoStack *m_stack    = nullptr;
    Timeline   *m_timeline = nullptr;
    Track      *m_track    = nullptr;
};

QTEST_MAIN(TestCommands)
#include "test_commands.moc"
