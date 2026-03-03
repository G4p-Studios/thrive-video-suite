// SPDX-License-Identifier: MIT
// Thrive Video Suite – ProjectSerializer round-trip tests

#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include "../src/core/project.h"
#include "../src/core/projectserializer.h"
#include "../src/core/timeline.h"
#include "../src/core/track.h"
#include "../src/core/clip.h"
#include "../src/core/effect.h"
#include "../src/core/marker.h"
#include "../src/core/transition.h"
#include "../src/core/timecode.h"

using namespace Thrive;

class TestSerializer : public QObject
{
    Q_OBJECT

private slots:

    // ── Round-trip: save → load with full project state ─────────────

    void roundTripEmptyProject()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        const QString path = tmpDir.filePath(QStringLiteral("empty.tvs"));

        // Save an empty project
        Project srcProject;
        srcProject.setName(QStringLiteral("Empty Test"));
        srcProject.setFps(30.0);
        srcProject.setResolution(1280, 720);

        // Remove default tracks to make truly empty
        auto *srcTl = srcProject.timeline();
        while (srcTl->trackCount() > 0)
            srcTl->removeTrack(0);

        ProjectSerializer serializer;
        QVERIFY(serializer.save(&srcProject, path));
        QVERIFY(QFile::exists(path));

        // Load into a fresh project
        Project dstProject;
        QVERIFY(serializer.load(&dstProject, path));

        QCOMPARE(dstProject.name(), QStringLiteral("Empty Test"));
        QCOMPARE(dstProject.fps(), 30.0);
        QCOMPARE(dstProject.width(), 1280);
        QCOMPARE(dstProject.height(), 720);
        QCOMPARE(dstProject.timeline()->trackCount(), 0);
        QCOMPARE(dstProject.timeline()->markers().size(), 0);
    }

    void roundTripFullProject()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        const QString path = tmpDir.filePath(QStringLiteral("full.tvs"));

        // ── Build a rich source project ──────────────────────────────
        Project srcProject;
        srcProject.setName(QStringLiteral("Round Trip"));
        srcProject.setFps(25.0);
        srcProject.setResolution(1920, 1080);
        srcProject.setScrubAudioEnabled(false);
        srcProject.setPreviewScale(480);

        auto *srcTl = srcProject.timeline();

        // Clear default tracks
        while (srcTl->trackCount() > 0)
            srcTl->removeTrack(0);

        // Video track with 2 clips
        auto *vTrack = new Track(QStringLiteral("Video 1"), Track::Type::Video);
        vTrack->setMuted(false);
        vTrack->setLocked(true);

        auto *clip1 = new Clip(QStringLiteral("Intro.mp4"),
                               QStringLiteral("/media/intro.mp4"),
                               TimeCode(0, 25.0), TimeCode(100, 25.0));
        clip1->setDescription(QStringLiteral("Opening shot"));
        clip1->setTimelinePosition(TimeCode(0, 25.0));

        // Effect with parameters
        auto *fx1 = new Effect(QStringLiteral("brightness"),
                               QStringLiteral("Brightness"),
                               QStringLiteral("Adjust brightness"));
        fx1->setEnabled(true);
        EffectParameter p1;
        p1.id = QStringLiteral("level");
        p1.displayName = QStringLiteral("Level");
        p1.type = QStringLiteral("float");
        p1.defaultValue = 1.0;
        p1.currentValue = 1.5;
        fx1->addParameter(p1);
        clip1->addEffect(fx1);

        // Out transition
        auto *trans1 = new Transition(QStringLiteral("luma"),
                                      QStringLiteral("Cross Dissolve"),
                                      QStringLiteral("Gradual blend"),
                                      TimeCode(25, 25.0));
        clip1->setOutTransition(trans1);

        vTrack->addClip(clip1);

        auto *clip2 = new Clip(QStringLiteral("Main.mp4"),
                               QStringLiteral("/media/main.mp4"),
                               TimeCode(10, 25.0), TimeCode(200, 25.0));
        clip2->setTimelinePosition(TimeCode(100, 25.0));

        // In transition
        auto *trans2 = new Transition(QStringLiteral("luma"),
                                      QStringLiteral("Cross Dissolve"),
                                      QStringLiteral("Gradual blend"),
                                      TimeCode(25, 25.0));
        clip2->setInTransition(trans2);

        vTrack->addClip(clip2);

        // Track-level effect
        auto *trackFx = new Effect(QStringLiteral("volume"),
                                   QStringLiteral("Volume"),
                                   QStringLiteral("Track volume"));
        trackFx->setEnabled(false);
        EffectParameter tp;
        tp.id = QStringLiteral("gain");
        tp.displayName = QStringLiteral("Gain");
        tp.type = QStringLiteral("float");
        tp.currentValue = 0.8;
        trackFx->addParameter(tp);
        vTrack->addTrackEffect(trackFx);

        srcTl->addTrack(vTrack);

        // Audio track (empty)
        auto *aTrack = new Track(QStringLiteral("Audio 1"), Track::Type::Audio);
        aTrack->setMuted(true);
        srcTl->addTrack(aTrack);

        // Markers
        srcTl->addMarker(new Marker(QStringLiteral("Chapter 1"),
                                    TimeCode(0, 25.0),
                                    QStringLiteral("Intro starts")));
        srcTl->addMarker(new Marker(QStringLiteral("Chapter 2"),
                                    TimeCode(100, 25.0),
                                    QStringLiteral("Main content")));

        // ── Save ─────────────────────────────────────────────────────
        ProjectSerializer serializer;
        QVERIFY(serializer.save(&srcProject, path));
        QVERIFY(QFile::exists(path));

        // ── Load into a fresh project ────────────────────────────────
        Project dstProject;
        QVERIFY(serializer.load(&dstProject, path));

        // Project metadata
        QCOMPARE(dstProject.name(), QStringLiteral("Round Trip"));
        QCOMPARE(dstProject.fps(), 25.0);
        QCOMPARE(dstProject.width(), 1920);
        QCOMPARE(dstProject.height(), 1080);
        QCOMPARE(dstProject.scrubAudioEnabled(), false);
        QCOMPARE(dstProject.previewScale(), 480);

        auto *dstTl = dstProject.timeline();

        // Tracks
        QCOMPARE(dstTl->trackCount(), 2);

        auto *dstV = dstTl->trackAt(0);
        QCOMPARE(dstV->name(), QStringLiteral("Video 1"));
        QCOMPARE(dstV->type(), Track::Type::Video);
        QCOMPARE(dstV->isMuted(), false);
        QCOMPARE(dstV->isLocked(), true);
        QCOMPARE(dstV->clipCount(), 2);

        auto *dstA = dstTl->trackAt(1);
        QCOMPARE(dstA->name(), QStringLiteral("Audio 1"));
        QCOMPARE(dstA->type(), Track::Type::Audio);
        QCOMPARE(dstA->isMuted(), true);
        QCOMPARE(dstA->clipCount(), 0);

        // Clip 1 details
        auto *dc1 = dstV->clipAt(0);
        QCOMPARE(dc1->name(), QStringLiteral("Intro.mp4"));
        QCOMPARE(dc1->sourcePath(), QStringLiteral("/media/intro.mp4"));
        QCOMPARE(dc1->description(), QStringLiteral("Opening shot"));
        QCOMPARE(dc1->inPoint().frame(), 0);
        QCOMPARE(dc1->outPoint().frame(), 100);
        QCOMPARE(dc1->timelinePosition().frame(), 0);

        // Effect on clip 1
        QCOMPARE(dc1->effects().size(), 1);
        auto *dFx1 = dc1->effects().at(0);
        QCOMPARE(dFx1->serviceId(), QStringLiteral("brightness"));
        QCOMPARE(dFx1->displayName(), QStringLiteral("Brightness"));
        QCOMPARE(dFx1->isEnabled(), true);

        // Out transition on clip 1
        QVERIFY(dc1->outTransition() != nullptr);
        QCOMPARE(dc1->outTransition()->serviceId(), QStringLiteral("luma"));
        QCOMPARE(dc1->outTransition()->duration().frame(), 25);
        QVERIFY(dc1->inTransition() == nullptr);

        // Clip 2 details
        auto *dc2 = dstV->clipAt(1);
        QCOMPARE(dc2->name(), QStringLiteral("Main.mp4"));
        QCOMPARE(dc2->inPoint().frame(), 10);
        QCOMPARE(dc2->outPoint().frame(), 200);

        // In transition on clip 2
        QVERIFY(dc2->inTransition() != nullptr);
        QCOMPARE(dc2->inTransition()->serviceId(), QStringLiteral("luma"));
        QVERIFY(dc2->outTransition() == nullptr);

        // Track-level effect
        QCOMPARE(dstV->trackEffects().size(), 1);
        auto *dTfx = dstV->trackEffects().at(0);
        QCOMPARE(dTfx->serviceId(), QStringLiteral("volume"));
        QCOMPARE(dTfx->isEnabled(), false);

        // Markers
        QCOMPARE(dstTl->markers().size(), 2);
        QCOMPARE(dstTl->markers().at(0)->name(), QStringLiteral("Chapter 1"));
        QCOMPARE(dstTl->markers().at(0)->position().frame(), 0);
        QCOMPARE(dstTl->markers().at(0)->comment(), QStringLiteral("Intro starts"));
        QCOMPARE(dstTl->markers().at(1)->name(), QStringLiteral("Chapter 2"));
        QCOMPARE(dstTl->markers().at(1)->position().frame(), 100);
    }

    void roundTripDoesNotDuplicateOnReload()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        const QString path = tmpDir.filePath(QStringLiteral("dup.tvs"));

        // Create a project with 1 track, 1 marker
        Project srcProject;
        auto *srcTl = srcProject.timeline();
        // reset() gives us default Video 1 + Audio 1

        srcTl->addMarker(new Marker(QStringLiteral("M1"),
                                    TimeCode(0, 25.0)));

        ProjectSerializer serializer;
        QVERIFY(serializer.save(&srcProject, path));

        // Load twice into the same project object
        QVERIFY(serializer.load(&srcProject, path));
        QVERIFY(serializer.load(&srcProject, path));

        // Should have exactly 2 tracks and 1 marker (not duplicated)
        QCOMPARE(srcProject.timeline()->trackCount(), 2);
        QCOMPARE(srcProject.timeline()->markers().size(), 1);
    }

    void effectParameterRoundTrip()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        const QString path = tmpDir.filePath(QStringLiteral("params.tvs"));

        Project srcProject;
        auto *srcTl = srcProject.timeline();
        while (srcTl->trackCount() > 0)
            srcTl->removeTrack(0);

        auto *track = new Track(QStringLiteral("V"), Track::Type::Video);
        auto *clip = new Clip(QStringLiteral("C"), {},
                              TimeCode(0, 25.0), TimeCode(50, 25.0));
        auto *fx = new Effect(QStringLiteral("color_correction"),
                              QStringLiteral("Color"),
                              QStringLiteral("Correction"));

        EffectParameter p1;
        p1.id = QStringLiteral("hue");
        p1.displayName = QStringLiteral("Hue");
        p1.type = QStringLiteral("float");
        p1.currentValue = 0.75;
        fx->addParameter(p1);

        EffectParameter p2;
        p2.id = QStringLiteral("saturation");
        p2.displayName = QStringLiteral("Saturation");
        p2.type = QStringLiteral("float");
        p2.currentValue = 1.2;
        fx->addParameter(p2);

        fx->setEnabled(false);
        clip->addEffect(fx);
        track->addClip(clip);
        srcTl->addTrack(track);

        ProjectSerializer serializer;
        QVERIFY(serializer.save(&srcProject, path));

        Project dstProject;
        QVERIFY(serializer.load(&dstProject, path));

        auto *dClip = dstProject.timeline()->trackAt(0)->clipAt(0);
        QCOMPARE(dClip->effects().size(), 1);
        auto *dFx = dClip->effects().at(0);
        QCOMPARE(dFx->serviceId(), QStringLiteral("color_correction"));
        QCOMPARE(dFx->isEnabled(), false);

        // Parameters are stored as string values via JSON
        QCOMPARE(dFx->parameterValue(QStringLiteral("hue")).toString(),
                 QStringLiteral("0.75"));
        QCOMPARE(dFx->parameterValue(QStringLiteral("saturation")).toString(),
                 QStringLiteral("1.2"));
    }

    void saveFailsOnBadPath()
    {
        Project project;
        ProjectSerializer serializer;
        // Try to save to an impossible path
        bool ok = serializer.save(&project,
            QStringLiteral("Z:/nonexistent/path/impossible.tvs"));
        QVERIFY(!ok);
        QVERIFY(!serializer.lastError().isEmpty());
    }

    void loadFailsOnMissingFile()
    {
        Project project;
        ProjectSerializer serializer;
        bool ok = serializer.load(&project,
            QStringLiteral("Z:/nonexistent.tvs"));
        QVERIFY(!ok);
        QVERIFY(!serializer.lastError().isEmpty());
    }
};

QTEST_MAIN(TestSerializer)
#include "test_serializer.moc"
