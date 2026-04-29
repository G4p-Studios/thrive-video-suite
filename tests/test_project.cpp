// SPDX-License-Identifier: MIT
// Thrive Video Suite – Project unit tests

#include <QTest>
#include <QSignalSpy>
#include "../src/core/project.h"
#include "../src/core/timeline.h"
#include "../src/core/track.h"

using namespace Thrive;

class TestProject : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_project = new Project(this);
    }

    void cleanup()
    {
        delete m_project;
        m_project = nullptr;
    }

    // ── defaults ────────────────────────────────────────────────────

    void defaultValues()
    {
        QCOMPARE(m_project->name(), tr("Untitled Project"));
        QVERIFY(m_project->filePath().isEmpty());
        QVERIFY(!m_project->isModified());
        QVERIFY(m_project->timeline() != nullptr);
        QCOMPARE(m_project->timeline()->trackCount(), 1);
        QCOMPARE(m_project->timeline()->trackAt(0)->name(), tr("Video 1"));
        QCOMPARE(m_project->fps(), 25.0);
        QCOMPARE(m_project->width(), 1920);
        QCOMPARE(m_project->height(), 1080);
        QVERIFY(m_project->scrubAudioEnabled());
        QCOMPARE(m_project->previewScale(), 640);
    }

    // ── name ────────────────────────────────────────────────────────

    void setNameEmitsSignal()
    {
        QSignalSpy spy(m_project, &Project::nameChanged);
        m_project->setName(QStringLiteral("My Video"));
        QCOMPARE(m_project->name(), QStringLiteral("My Video"));
        QCOMPARE(spy.count(), 1);
    }

    // ── modified flag ───────────────────────────────────────────────

    void setModified()
    {
        QSignalSpy spy(m_project, &Project::modifiedChanged);
        m_project->setModified(true);
        QVERIFY(m_project->isModified());
        QCOMPARE(spy.count(), 1);

        m_project->setModified(true); // same
        QCOMPARE(spy.count(), 1);     // no dup
    }

    // ── resolution ──────────────────────────────────────────────────

    void setResolution()
    {
        QSignalSpy spy(m_project, &Project::settingsChanged);
        m_project->setResolution(3840, 2160);
        QCOMPARE(m_project->width(), 3840);
        QCOMPARE(m_project->height(), 2160);
        QCOMPARE(spy.count(), 1);
    }

    void setFps()
    {
        m_project->setFps(30.0);
        QCOMPARE(m_project->fps(), 30.0);
    }

    // ── preview / scrub settings ────────────────────────────────────

    void scrubAudio()
    {
        m_project->setScrubAudioEnabled(false);
        QVERIFY(!m_project->scrubAudioEnabled());

        m_project->setScrubAudioEnabled(true);
        QVERIFY(m_project->scrubAudioEnabled());
    }

    void previewScale()
    {
        m_project->setPreviewScale(480);
        QCOMPARE(m_project->previewScale(), 480);
    }

    // ── reset ───────────────────────────────────────────────────────

    void reset()
    {
        m_project->setName(QStringLiteral("Test"));
        m_project->setModified(true);
        m_project->setResolution(720, 480);
        m_project->setFps(30.0);
        m_project->setScrubAudioEnabled(false);

        m_project->reset();

        QCOMPARE(m_project->name(), tr("Untitled Project"));
        QVERIFY(!m_project->isModified());
        QCOMPARE(m_project->width(), 1920);
        QCOMPARE(m_project->height(), 1080);
        QCOMPARE(m_project->fps(), 25.0);
        QVERIFY(m_project->scrubAudioEnabled());
        QVERIFY(m_project->timeline() != nullptr);
        QCOMPARE(m_project->timeline()->trackCount(), 1);
        QCOMPARE(m_project->timeline()->trackAt(0)->name(), tr("Video 1"));
    }

    // ── timeline ownership ──────────────────────────────────────────

    void timelineIsOwned()
    {
        // Timeline should be a child of Project
        QCOMPARE(m_project->timeline()->parent(), m_project);
    }

private:
    Project *m_project = nullptr;
};

QTEST_MAIN(TestProject)
#include "test_project.moc"
