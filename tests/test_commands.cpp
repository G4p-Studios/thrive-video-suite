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
#include "../src/core/transition.h"
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

    // ── MoveClipBetweenTracksCommand ────────────────────────────────

    void moveClipBetweenTracksUndoRedo()
    {
        auto *c1 = new Clip(QStringLiteral("Traveller"), {},
                            TimeCode(0, 25.0), TimeCode(50, 25.0));
        m_track->addClip(c1);

        auto *audioTrack = new Track(QStringLiteral("Audio 1"),
                                     Track::Type::Audio);
        m_timeline->addTrack(audioTrack);

        QCOMPARE(m_track->clipCount(), 1);
        QCOMPARE(audioTrack->clipCount(), 0);

        m_stack->push(new MoveClipBetweenTracksCommand(
            m_track, 0, audioTrack));

        QCOMPARE(m_track->clipCount(), 0);
        QCOMPARE(audioTrack->clipCount(), 1);
        QCOMPARE(audioTrack->clipAt(0)->name(), QStringLiteral("Traveller"));

        m_stack->undo();
        QCOMPARE(m_track->clipCount(), 1);
        QCOMPARE(audioTrack->clipCount(), 0);

        m_stack->redo();
        QCOMPARE(m_track->clipCount(), 0);
        QCOMPARE(audioTrack->clipCount(), 1);
    }

    // ── RenameTrackCommand ──────────────────────────────────────────

    void renameTrackUndoRedo()
    {
        QCOMPARE(m_track->name(), QStringLiteral("Video 1"));

        m_stack->push(new RenameTrackCommand(
            m_track, QStringLiteral("Main Video")));

        QCOMPARE(m_track->name(), QStringLiteral("Main Video"));

        m_stack->undo();
        QCOMPARE(m_track->name(), QStringLiteral("Video 1"));

        m_stack->redo();
        QCOMPARE(m_track->name(), QStringLiteral("Main Video"));
    }

    // ── MoveEffectCommand ───────────────────────────────────────────

    void moveEffectUndoRedo()
    {
        auto *clip = new Clip(QStringLiteral("FxClip"), {},
                              TimeCode(), TimeCode(50, 25.0));
        m_track->addClip(clip);

        auto *fx1 = new Effect(QStringLiteral("blur"),
                               QStringLiteral("Blur"), {});
        auto *fx2 = new Effect(QStringLiteral("sharpen"),
                               QStringLiteral("Sharpen"), {});
        auto *fx3 = new Effect(QStringLiteral("color"),
                               QStringLiteral("Color"), {});
        clip->addEffect(fx1);
        clip->addEffect(fx2);
        clip->addEffect(fx3);

        // Move blur from 0 to 2
        m_stack->push(new MoveEffectCommand(clip, 0, 2));
        QCOMPARE(clip->effects().at(0)->serviceId(), QStringLiteral("sharpen"));
        QCOMPARE(clip->effects().at(2)->serviceId(), QStringLiteral("blur"));

        m_stack->undo();
        QCOMPARE(clip->effects().at(0)->serviceId(), QStringLiteral("blur"));
        QCOMPARE(clip->effects().at(2)->serviceId(), QStringLiteral("color"));
    }

    // ── ChangeTransitionDurationCommand ─────────────────────────────

    void changeTransitionDurationUndoRedo()
    {
        auto *trans = new Transition(QStringLiteral("luma"),
                                     QStringLiteral("Dissolve"),
                                     {},
                                     TimeCode(25, 25.0));

        m_stack->push(new ChangeTransitionDurationCommand(
            trans, TimeCode(50, 25.0)));

        QCOMPARE(trans->duration().frame(), 50);

        m_stack->undo();
        QCOMPARE(trans->duration().frame(), 25);

        m_stack->redo();
        QCOMPARE(trans->duration().frame(), 50);

        delete trans;
    }

    void changeTransitionDurationMerge()
    {
        auto *trans = new Transition(QStringLiteral("luma"),
                                     QStringLiteral("Dissolve"),
                                     {},
                                     TimeCode(10, 25.0));

        m_stack->push(new ChangeTransitionDurationCommand(
            trans, TimeCode(20, 25.0)));
        m_stack->push(new ChangeTransitionDurationCommand(
            trans, TimeCode(30, 25.0)));

        // Merged: a single undo should restore to 10
        QCOMPARE(trans->duration().frame(), 30);
        m_stack->undo();
        QCOMPARE(trans->duration().frame(), 10);

        delete trans;
    }

    // ── RemoveEffectCommand preserves order ─────────────────────────

    void removeEffectPreservesOrder()
    {
        auto *clip = new Clip(QStringLiteral("OrderClip"), {},
                              TimeCode(), TimeCode(50, 25.0));
        m_track->addClip(clip);

        auto *fx1 = new Effect(QStringLiteral("a"), QStringLiteral("A"), {});
        auto *fx2 = new Effect(QStringLiteral("b"), QStringLiteral("B"), {});
        auto *fx3 = new Effect(QStringLiteral("c"), QStringLiteral("C"), {});
        clip->addEffect(fx1);
        clip->addEffect(fx2);
        clip->addEffect(fx3);

        // Remove the middle effect
        m_stack->push(new RemoveEffectCommand(clip, 1));
        QCOMPARE(clip->effects().size(), 2);
        QCOMPARE(clip->effects().at(0)->serviceId(), QStringLiteral("a"));
        QCOMPARE(clip->effects().at(1)->serviceId(), QStringLiteral("c"));

        // Undo should restore B at index 1
        m_stack->undo();
        QCOMPARE(clip->effects().size(), 3);
        QCOMPARE(clip->effects().at(0)->serviceId(), QStringLiteral("a"));
        QCOMPARE(clip->effects().at(1)->serviceId(), QStringLiteral("b"));
        QCOMPARE(clip->effects().at(2)->serviceId(), QStringLiteral("c"));
    }

    // ── Deep-copy fidelity (clipboard operations) ───────────────────

    void deepCopyPreservesEffectParameters()
    {
        // Simulate what cut/copy does: create a clip with effects+params,
        // deep-copy it, verify the copy has all metadata.
        auto *src = new Clip(QStringLiteral("Source"), QStringLiteral("/vid.mp4"),
                             TimeCode(10, 25.0), TimeCode(100, 25.0));
        src->setDescription(QStringLiteral("Important clip"));
        src->setTimelinePosition(TimeCode(50, 25.0));

        auto *fx = new Effect(QStringLiteral("blur"),
                              QStringLiteral("Gaussian Blur"),
                              QStringLiteral("Blur effect"));
        fx->setEnabled(false);
        EffectParameter p;
        p.id = QStringLiteral("radius");
        p.displayName = QStringLiteral("Radius");
        p.type = QStringLiteral("float");
        p.defaultValue = 5.0;
        p.currentValue = 12.5;
        p.minimum = 0.0;
        p.maximum = 100.0;
        fx->addParameter(p);
        src->addEffect(fx);

        // Add transitions
        auto *outT = new Transition(QStringLiteral("luma"),
                                    QStringLiteral("Dissolve"),
                                    QStringLiteral("Cross dissolve"),
                                    TimeCode(25, 25.0));
        src->setOutTransition(outT);

        // Perform deep copy (same as clipboard code)
        auto *copy = new Clip(src->name(), src->sourcePath(),
                              src->inPoint(), src->outPoint());
        copy->setDescription(src->description());
        copy->setTimelinePosition(src->timelinePosition());
        for (auto *sfx : src->effects()) {
            auto *fxCopy = new Effect(sfx->serviceId(), sfx->displayName(),
                                      sfx->description(), copy);
            fxCopy->setEnabled(sfx->isEnabled());
            for (const auto &param : sfx->parameters())
                fxCopy->addParameter(param);
            copy->addEffect(fxCopy);
        }
        if (auto *t = src->outTransition())
            copy->setOutTransition(
                new Transition(t->serviceId(), t->displayName(),
                               t->description(), t->duration(), copy));
        if (auto *t = src->inTransition())
            copy->setInTransition(
                new Transition(t->serviceId(), t->displayName(),
                               t->description(), t->duration(), copy));

        // Verify deep copy
        QCOMPARE(copy->name(), QStringLiteral("Source"));
        QCOMPARE(copy->sourcePath(), QStringLiteral("/vid.mp4"));
        QCOMPARE(copy->description(), QStringLiteral("Important clip"));
        QCOMPARE(copy->inPoint().frame(), 10);
        QCOMPARE(copy->outPoint().frame(), 100);
        QCOMPARE(copy->timelinePosition().frame(), 50);

        // Effects
        QCOMPARE(copy->effects().size(), 1);
        auto *cfx = copy->effects().at(0);
        QCOMPARE(cfx->serviceId(), QStringLiteral("blur"));
        QCOMPARE(cfx->isEnabled(), false);
        QCOMPARE(cfx->parameters().size(), 1);
        QCOMPARE(cfx->parameters().at(0).id, QStringLiteral("radius"));
        QCOMPARE(cfx->parameters().at(0).currentValue.toDouble(), 12.5);

        // Out transition
        QVERIFY(copy->outTransition() != nullptr);
        QCOMPARE(copy->outTransition()->serviceId(), QStringLiteral("luma"));
        QCOMPARE(copy->outTransition()->duration().frame(), 25);

        // In transition should be null (src didn't have one)
        QVERIFY(copy->inTransition() == nullptr);

        // Verify independence: changing source doesn't affect copy
        src->setDescription(QStringLiteral("Changed"));
        QCOMPARE(copy->description(), QStringLiteral("Important clip"));

        delete src;
        delete copy;
    }

    void deepCopyPreservesMultipleEffects()
    {
        auto *src = new Clip(QStringLiteral("Multi"), {},
                             TimeCode(0, 25.0), TimeCode(50, 25.0));

        auto *fx1 = new Effect(QStringLiteral("brightness"),
                               QStringLiteral("Brightness"), {});
        auto *fx2 = new Effect(QStringLiteral("contrast"),
                               QStringLiteral("Contrast"), {});
        auto *fx3 = new Effect(QStringLiteral("saturation"),
                               QStringLiteral("Saturation"), {});
        fx2->setEnabled(false);
        src->addEffect(fx1);
        src->addEffect(fx2);
        src->addEffect(fx3);

        // Deep copy
        auto *copy = new Clip(src->name(), src->sourcePath(),
                              src->inPoint(), src->outPoint());
        for (auto *sfx : src->effects()) {
            auto *fxCopy = new Effect(sfx->serviceId(), sfx->displayName(),
                                      sfx->description(), copy);
            fxCopy->setEnabled(sfx->isEnabled());
            copy->addEffect(fxCopy);
        }

        QCOMPARE(copy->effects().size(), 3);
        QCOMPARE(copy->effects().at(0)->serviceId(), QStringLiteral("brightness"));
        QCOMPARE(copy->effects().at(0)->isEnabled(), true);
        QCOMPARE(copy->effects().at(1)->serviceId(), QStringLiteral("contrast"));
        QCOMPARE(copy->effects().at(1)->isEnabled(), false);
        QCOMPARE(copy->effects().at(2)->serviceId(), QStringLiteral("saturation"));
        QCOMPARE(copy->effects().at(2)->isEnabled(), true);

        delete src;
        delete copy;
    }

    // ── AddTransitionCommand / RemoveTransitionCommand ──────────────

    void addTransitionUndoRedo()
    {
        auto *clip = new Clip(QStringLiteral("TransClip"), {},
                              TimeCode(0, 25.0), TimeCode(50, 25.0));
        m_track->addClip(clip);

        auto *trans = new Transition(QStringLiteral("luma"),
                                     QStringLiteral("Dissolve"),
                                     QStringLiteral("Cross dissolve"),
                                     TimeCode(10, 25.0));

        QVERIFY(clip->outTransition() == nullptr);

        m_stack->push(new AddTransitionCommand(
            clip, AddTransitionCommand::Edge::Out, trans));
        QVERIFY(clip->outTransition() != nullptr);
        QCOMPARE(clip->outTransition()->serviceId(), QStringLiteral("luma"));

        m_stack->undo();
        QVERIFY(clip->outTransition() == nullptr);

        m_stack->redo();
        QVERIFY(clip->outTransition() != nullptr);
        QCOMPARE(clip->outTransition()->duration().frame(), 10);
    }

    void removeTransitionUndoRedo()
    {
        auto *clip = new Clip(QStringLiteral("TransClip"), {},
                              TimeCode(0, 25.0), TimeCode(50, 25.0));
        m_track->addClip(clip);

        auto *trans = new Transition(QStringLiteral("wipe"),
                                     QStringLiteral("Wipe"),
                                     {},
                                     TimeCode(15, 25.0));
        clip->setInTransition(trans);
        QVERIFY(clip->inTransition() != nullptr);

        m_stack->push(new RemoveTransitionCommand(
            clip, RemoveTransitionCommand::Edge::In));
        QVERIFY(clip->inTransition() == nullptr);

        m_stack->undo();
        QVERIFY(clip->inTransition() != nullptr);
        QCOMPARE(clip->inTransition()->serviceId(), QStringLiteral("wipe"));

        m_stack->redo();
        QVERIFY(clip->inTransition() == nullptr);
    }

    // ── SetEffectEnabledCommand ─────────────────────────────────────

    void setEffectEnabledUndoRedo()
    {
        auto *clip = new Clip(QStringLiteral("FxClip"), {},
                              TimeCode(), TimeCode(50, 25.0));
        m_track->addClip(clip);

        auto *fx = new Effect(QStringLiteral("blur"),
                              QStringLiteral("Blur"), {});
        clip->addEffect(fx);
        QCOMPARE(fx->isEnabled(), true);

        m_stack->push(new SetEffectEnabledCommand(fx, false));
        QCOMPARE(fx->isEnabled(), false);

        m_stack->undo();
        QCOMPARE(fx->isEnabled(), true);

        m_stack->redo();
        QCOMPARE(fx->isEnabled(), false);
    }

    // ── ChangeEffectParameterCommand + merge ────────────────────────

    void changeEffectParameterUndoRedo()
    {
        auto *clip = new Clip(QStringLiteral("ParamClip"), {},
                              TimeCode(), TimeCode(50, 25.0));
        m_track->addClip(clip);

        auto *fx = new Effect(QStringLiteral("brightness"),
                              QStringLiteral("Brightness"), {});
        EffectParameter p;
        p.id = QStringLiteral("level");
        p.displayName = QStringLiteral("Level");
        p.type = QStringLiteral("float");
        p.defaultValue = 1.0;
        p.currentValue = 1.0;
        p.minimum = 0.0;
        p.maximum = 2.0;
        fx->addParameter(p);
        clip->addEffect(fx);

        m_stack->push(new ChangeEffectParameterCommand(
            fx, QStringLiteral("level"), 1.5));
        QCOMPARE(fx->parameterValue(QStringLiteral("level")).toDouble(), 1.5);

        m_stack->undo();
        QCOMPARE(fx->parameterValue(QStringLiteral("level")).toDouble(), 1.0);
    }

    void changeEffectParameterMerge()
    {
        auto *clip = new Clip(QStringLiteral("MergeClip"), {},
                              TimeCode(), TimeCode(50, 25.0));
        m_track->addClip(clip);

        auto *fx = new Effect(QStringLiteral("volume"),
                              QStringLiteral("Volume"), {});
        EffectParameter p;
        p.id = QStringLiteral("gain");
        p.displayName = QStringLiteral("Gain");
        p.type = QStringLiteral("float");
        p.defaultValue = 0.0;
        p.currentValue = 0.0;
        p.minimum = -60.0;
        p.maximum = 20.0;
        fx->addParameter(p);
        clip->addEffect(fx);

        m_stack->push(new ChangeEffectParameterCommand(
            fx, QStringLiteral("gain"), 5.0));
        m_stack->push(new ChangeEffectParameterCommand(
            fx, QStringLiteral("gain"), 10.0));

        // Merged: single undo restores to original 0.0
        QCOMPARE(fx->parameterValue(QStringLiteral("gain")).toDouble(), 10.0);
        m_stack->undo();
        QCOMPARE(fx->parameterValue(QStringLiteral("gain")).toDouble(), 0.0);
    }

    // ── MoveTrackCommand ────────────────────────────────────────────

    void moveTrackUndoRedo()
    {
        auto *audio = new Track(QStringLiteral("Audio 1"), Track::Type::Audio);
        m_timeline->addTrack(audio);
        // Timeline now has [Video 1, Audio 1] at indices 0, 1
        QCOMPARE(m_timeline->trackAt(0)->name(), QStringLiteral("Video 1"));
        QCOMPARE(m_timeline->trackAt(1)->name(), QStringLiteral("Audio 1"));

        m_stack->push(new MoveTrackCommand(m_timeline, 0, 1));
        QCOMPARE(m_timeline->trackAt(0)->name(), QStringLiteral("Audio 1"));
        QCOMPARE(m_timeline->trackAt(1)->name(), QStringLiteral("Video 1"));

        m_stack->undo();
        QCOMPARE(m_timeline->trackAt(0)->name(), QStringLiteral("Video 1"));
        QCOMPARE(m_timeline->trackAt(1)->name(), QStringLiteral("Audio 1"));
    }

    // ── RenameClipCommand ───────────────────────────────────────────

    void renameClipUndoRedo()
    {
        auto *clip = new Clip(QStringLiteral("Original"), {},
                              TimeCode(), TimeCode(50, 25.0));
        m_track->addClip(clip);
        QCOMPARE(clip->name(), QStringLiteral("Original"));

        m_stack->push(new RenameClipCommand(clip, QStringLiteral("Renamed")));
        QCOMPARE(clip->name(), QStringLiteral("Renamed"));

        m_stack->undo();
        QCOMPARE(clip->name(), QStringLiteral("Original"));

        m_stack->redo();
        QCOMPARE(clip->name(), QStringLiteral("Renamed"));
    }

    // ── ChangeClipDescriptionCommand ────────────────────────────────

    void changeClipDescriptionUndoRedo()
    {
        auto *clip = new Clip(QStringLiteral("DescClip"), {},
                              TimeCode(), TimeCode(50, 25.0));
        clip->setDescription(QStringLiteral("Old description"));
        m_track->addClip(clip);

        m_stack->push(new ChangeClipDescriptionCommand(
            clip, QStringLiteral("New description")));
        QCOMPARE(clip->description(), QStringLiteral("New description"));

        m_stack->undo();
        QCOMPARE(clip->description(), QStringLiteral("Old description"));
    }

    // ── NudgeClipPositionCommand ────────────────────────────────────

    void nudgeClipPositionUndoRedo()
    {
        auto *clip = new Clip(QStringLiteral("NudgeClip"), {},
                              TimeCode(0, 25.0), TimeCode(50, 25.0));
        clip->setTimelinePosition(TimeCode(100, 25.0));
        m_track->addClip(clip);

        m_stack->push(new NudgeClipPositionCommand(
            clip, TimeCode(101, 25.0)));
        QCOMPARE(clip->timelinePosition().frame(), 101);

        m_stack->undo();
        QCOMPARE(clip->timelinePosition().frame(), 100);

        m_stack->redo();
        QCOMPARE(clip->timelinePosition().frame(), 101);
    }

    void nudgeClipPositionMerge()
    {
        auto *clip = new Clip(QStringLiteral("NudgeClip"), {},
                              TimeCode(0, 25.0), TimeCode(50, 25.0));
        clip->setTimelinePosition(TimeCode(100, 25.0));
        m_track->addClip(clip);

        m_stack->push(new NudgeClipPositionCommand(
            clip, TimeCode(101, 25.0)));
        m_stack->push(new NudgeClipPositionCommand(
            clip, TimeCode(102, 25.0)));
        m_stack->push(new NudgeClipPositionCommand(
            clip, TimeCode(103, 25.0)));

        // All three merged: single undo restores to 100
        QCOMPARE(clip->timelinePosition().frame(), 103);
        m_stack->undo();
        QCOMPARE(clip->timelinePosition().frame(), 100);
    }

    // ── SplitClipCommand with non-zero inPoint (source offset fix) ──

    void splitClipSourceOffset()
    {
        // Clip with inPoint=20, outPoint=120, at timeline position 200
        auto *clip = new Clip(QStringLiteral("Offset"), {},
                              TimeCode(20, 25.0), TimeCode(120, 25.0));
        clip->setTimelinePosition(TimeCode(200, 25.0));
        m_track->addClip(clip);

        // Split at timeline frame 250 → 50 frames into the clip
        // Source split should be inPoint(20) + 50 = 70
        m_stack->push(new SplitClipCommand(m_track, 0, TimeCode(250, 25.0)));

        QCOMPARE(m_track->clipCount(), 2);
        // Original: in=20, out=70 (source-relative)
        QCOMPARE(m_track->clipAt(0)->inPoint().frame(), 20);
        QCOMPARE(m_track->clipAt(0)->outPoint().frame(), 70);
        // New clip: in=70, out=120 (source-relative)
        QCOMPARE(m_track->clipAt(1)->inPoint().frame(), 70);
        QCOMPARE(m_track->clipAt(1)->outPoint().frame(), 120);
        // New clip is placed at the split timeline position
        QCOMPARE(m_track->clipAt(1)->timelinePosition().frame(), 250);

        m_stack->undo();
        QCOMPARE(m_track->clipCount(), 1);
        QCOMPARE(m_track->clipAt(0)->inPoint().frame(), 20);
        QCOMPARE(m_track->clipAt(0)->outPoint().frame(), 120);
    }

private:
    QUndoStack *m_stack    = nullptr;
    Timeline   *m_timeline = nullptr;
    Track      *m_track    = nullptr;
};

QTEST_MAIN(TestCommands)
#include "test_commands.moc"
