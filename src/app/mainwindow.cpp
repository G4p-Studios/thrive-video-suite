// SPDX-License-Identifier: MIT
// Thrive Video Suite – Main window implementation

#include "mainwindow.h"
#include "constants.h"
#include "shortcutmanager.h"
#include "welcomewizard.h"

#include "../core/project.h"
#include "../core/timeline.h"
#include "../core/track.h"
#include "../core/clip.h"
#include "../core/effect.h"
#include "../core/marker.h"
#include "../core/commands.h"
#include "../core/projectserializer.h"
#include "../core/transition.h"
#include "../engine/mltengine.h"
#include "../engine/playbackcontroller.h"
#include "../engine/renderengine.h"
#include "../engine/effectcatalog.h"
#include "../engine/tractorbuilder.h"
#include "../accessibility/announcer.h"
#include "../accessibility/audiocuemanager.h"

#include "../ui/timelinewidget.h"
#include "../ui/transportbar.h"
#include "../ui/mediabrowser.h"
#include "../ui/propertiespanel.h"
#include "../ui/effectsbrowser.h"
#include "../ui/preferencesdialog.h"
#include "../ui/shortcuteditor.h"

#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>
#include <mlt++/MltTractor.h>

#include <QAction>
#include <QMenuBar>
#include <QMenu>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QSettings>
#include <QApplication>
#include <QTimer>

namespace Thrive {

MainWindow::MainWindow(Project *project,
                       MltEngine *engine,
                       Announcer *announcer,
                       AudioCueManager *cues,
                       QWidget *parent)
    : QMainWindow(parent)
    , m_project(project)
    , m_engine(engine)
    , m_announcer(announcer)
    , m_cues(cues)
    , m_undoStack(new QUndoStack(this))
    , m_serializer(new ProjectSerializer(this))
{
    setWindowTitle(tr("Thrive Video Suite"));
    setMinimumSize(900, 600);
    setAccessibleName(tr("Thrive Video Suite main window"));

    // Engine subsystems
    m_playback       = new PlaybackController(m_engine, this);
    m_render         = new RenderEngine(m_engine, this);
    m_catalog        = new EffectCatalog(m_engine, this);
    m_tractorBuilder = new TractorBuilder(m_engine, this);
    m_catalog->refresh();

    // Load audio cues so they actually play during navigation
    m_cues->loadCues();

    // Rebuild the MLT pipeline whenever the timeline model changes
    connect(m_project->timeline(), &Timeline::tracksChanged,
            this, &MainWindow::rebuildTractor);

    // When a new tractor is ready, reconnect playback
    connect(m_tractorBuilder, &TractorBuilder::tractorReady,
            this, [this](Mlt::Tractor *tractor) {
                m_playback->close();
                m_playback->setProducer(tractor);
                m_playback->open();
            });

    // Sync playback position → timeline playhead
    connect(m_playback, &PlaybackController::positionChanged,
            this, [this](int frame) {
                m_project->timeline()->setPlayheadPosition(
                    TimeCode(frame, m_project->fps()));
            });

    // Sync modified flag with Project model & update window title
    connect(m_project, &Project::modifiedChanged,
            this, [this](bool modified) {
                m_modified = modified;
                updateWindowTitle();
            });
    connect(m_project, &Project::nameChanged,
            this, [this](const QString &) { updateWindowTitle(); });
    connect(m_undoStack, &QUndoStack::indexChanged,
            this, [this]() {
                m_project->setModified(true);
            });

    createActions();
    createMenus();
    createDockWidgets();
    registerShortcuts();

    // Load saved shortcuts
    ShortcutManager::instance().load();

    // Post-restart plugin announcement
    checkPostRestartPlugins();

    // First-run wizard
    checkFirstRun();
}

// ── close ────────────────────────────────────────────────────────────

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_modified) {
        auto answer = QMessageBox::question(
            this, tr("Unsaved Changes"),
            tr("Save changes before closing?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

        if (answer == QMessageBox::Save) {
            saveProject();
        } else if (answer == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
    }
    event->accept();
}

// ── file actions ─────────────────────────────────────────────────────

void MainWindow::newProject()
{
    m_playback->close();
    m_project->reset();
    m_currentFilePath.clear();
    m_modified = false;
    m_undoStack->clear();

    // Project::reset() replaces the Timeline object, so reconnect everything
    reconnectTimeline();

    m_timeline->refresh();
    updateWindowTitle();
    m_announcer->announce(tr("New project created."),
                          Announcer::Priority::High);
}

void MainWindow::openProject()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Project"), QString(),
        tr("TVS Projects (*.tvs);;All Files (*)"));
    if (path.isEmpty()) return;

    m_playback->close();

    if (!m_serializer->load(m_project, path)) {
        QMessageBox::warning(this, tr("Open Failed"),
                             m_serializer->lastError());
        return;
    }

    m_currentFilePath = path;
    m_modified = false;
    m_undoStack->clear();

    // Load may replace the Timeline, so reconnect
    reconnectTimeline();

    rebuildTractor();
    m_timeline->refresh();
    updateWindowTitle();
    m_announcer->announce(
        tr("Project opened: %1").arg(path), Announcer::Priority::High);
}

void MainWindow::saveProject()
{
    if (m_currentFilePath.isEmpty()) {
        saveProjectAs();
        return;
    }

    // Serialize the live MLT tractor to XML before writing the file
    m_serializer->setMltXml(m_tractorBuilder->serializeToXml());

    if (!m_serializer->save(m_project, m_currentFilePath)) {
        QMessageBox::warning(this, tr("Save Failed"),
                             m_serializer->lastError());
        return;
    }

    m_modified = false;
    m_project->setModified(false);
    updateWindowTitle();
    m_announcer->announce(tr("Project saved."), Announcer::Priority::Normal);
}

void MainWindow::saveProjectAs()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save Project As"), QString(),
        tr("TVS Projects (*.tvs)"));
    if (path.isEmpty()) return;

    m_currentFilePath = path;
    saveProject();
}

void MainWindow::exportVideo()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export Video"), QString(),
        tr("MP4 Video (*.mp4);;MKV Video (*.mkv);;All Files (*)"));
    if (path.isEmpty()) return;

    m_announcer->announce(tr("Export started…"), Announcer::Priority::High);
    // Disconnect previous render connections to avoid accumulating duplicates
    disconnect(m_render, &RenderEngine::renderProgress, this, nullptr);
    disconnect(m_render, &RenderEngine::renderFinished, this, nullptr);
    connect(m_render, &RenderEngine::renderProgress,
            this, [this](int percent) {
                m_announcer->announce(
                    tr("Rendering: %1%").arg(percent),
                    Announcer::Priority::Low);
            });
    connect(m_render, &RenderEngine::renderFinished,
            this, [this](bool success) {
                m_announcer->announce(
                    success ? tr("Export complete!")
                            : tr("Export failed."),
                    Announcer::Priority::High);
            });

    auto *tractor = m_tractorBuilder->tractor();
    if (!tractor) {
        m_announcer->announce(
            tr("Nothing to export. Add clips to the timeline first."),
            Announcer::Priority::High);
        return;
    }

    // Pause playback before rendering
    m_playback->pause();

    m_render->startRender(tractor, path,
                          QStringLiteral("mp4"),
                          QStringLiteral("libx264"),
                          QStringLiteral("aac"));
}

void MainWindow::showPreferences()
{
    auto *dlg = new PreferencesDialog(m_project, m_announcer, this);

    // Load current shortcuts into editor
    dlg->shortcutEditor()->loadShortcuts(
        ShortcutManager::instance().toMap());

    connect(dlg, &PreferencesDialog::previewScaleChanged,
            this, [this](int h) {
                m_engine->setPreviewScale(h);
                // Compute width preserving aspect ratio (16:9)
                int w = h * m_project->width() / m_project->height();
                w += w % 2; // ensure even
                m_playback->applyPreviewScale(w, h);
            });
    connect(dlg, &PreferencesDialog::audioCuesEnabledChanged,
            m_cues, &AudioCueManager::setEnabled);
    connect(dlg, &PreferencesDialog::audioCueVolumeChanged,
            m_cues, &AudioCueManager::setVolume);
    connect(dlg, &PreferencesDialog::restartRequired,
            this, [this]() {
                QSettings().setValue(
                    QLatin1String(kSettingsPluginJustInstalled), true);
                auto answer = QMessageBox::question(
                    this, tr("Restart Required"),
                    tr("A restart is needed for changes to take effect. "
                       "Restart now?"));
                if (answer == QMessageBox::Yes) {
                    QApplication::exit(EXIT_RESTART);
                }
            });

    if (dlg->exec() == QDialog::Accepted) {
        ShortcutManager::instance().applyMap(
            dlg->shortcutEditor()->shortcuts());
    }
    dlg->deleteLater();
}

void MainWindow::showWelcomeWizard()
{
    auto *wiz = new WelcomeWizard(m_announcer, this);
    wiz->exec();
    wiz->deleteLater();
}

void MainWindow::showAbout()
{
    QMessageBox::about(
        this,
        tr("About Thrive Video Suite"),
        tr("<h2>Thrive Video Suite %1</h2>"
           "<p>A fully accessible video editor for blind and visually "
           "impaired users.</p>"
           "<p>Built with Qt, MLT Framework, and Prism.</p>")
            .arg(QLatin1String(kAppVersion)));
}

// ── action / menu setup ──────────────────────────────────────────────

void MainWindow::createActions()
{
    m_actNew   = new QAction(tr("&New Project"),  this);
    m_actOpen  = new QAction(tr("&Open…"),        this);
    m_actSave  = new QAction(tr("&Save"),         this);
    m_actSaveAs = new QAction(tr("Save &As…"),    this);
    m_actExport = new QAction(tr("&Export…"),      this);
    m_actPreferences = new QAction(tr("Pr&eferences…"), this);
    m_actWizard = new QAction(tr("Welcome &Wizard"), this);
    m_actAbout = new QAction(tr("&About"),         this);
    m_actQuit  = new QAction(tr("&Quit"),          this);

    m_actUndo = m_undoStack->createUndoAction(this, tr("&Undo"));
    m_actRedo = m_undoStack->createRedoAction(this, tr("&Redo"));
    m_actUndo->setShortcut(QKeySequence::Undo);
    m_actRedo->setShortcut(QKeySequence::Redo);

    m_actCut       = new QAction(tr("Cu&t"),        this);
    m_actCopy      = new QAction(tr("&Copy"),       this);
    m_actPaste     = new QAction(tr("&Paste"),      this);
    m_actDelete    = new QAction(tr("&Delete"),     this);
    m_actSelectAll = new QAction(tr("Select &All"), this);

    m_actCut->setShortcut(QKeySequence::Cut);
    m_actCopy->setShortcut(QKeySequence::Copy);
    m_actPaste->setShortcut(QKeySequence::Paste);
    m_actDelete->setShortcut(QKeySequence::Delete);
    m_actSelectAll->setShortcut(QKeySequence::SelectAll);

    // ── Clipboard operations ─────────────────────────────────────────
    connect(m_actCut, &QAction::triggered, this, [this]() {
        auto *tl  = m_project->timeline();
        auto *trk = tl->trackAt(tl->currentTrackIndex());
        int   idx = tl->currentClipIndex();
        if (!trk || idx < 0 || idx >= trk->clipCount()) {
            m_announcer->announce(tr("No clip selected."),
                                  Announcer::Priority::Normal);
            return;
        }
        // Copy to clipboard before removing
        auto *src = trk->clipAt(idx);
        delete m_clipboardClip;
        m_clipboardClip = new Clip(src->name(), src->sourcePath(),
                                   src->inPoint(), src->outPoint(), this);
        m_clipboardClip->setTimelinePosition(src->timelinePosition());
        for (auto *fx : src->effects())
            m_clipboardClip->addEffect(
                new Effect(fx->serviceId(), fx->displayName(),
                           fx->description(), m_clipboardClip));

        m_undoStack->push(new RemoveClipCommand(trk, idx));
        m_modified = true;
        m_announcer->announce(
            tr("Cut: %1").arg(src->name()), Announcer::Priority::Normal);
    });

    connect(m_actCopy, &QAction::triggered, this, [this]() {
        auto *tl  = m_project->timeline();
        auto *trk = tl->trackAt(tl->currentTrackIndex());
        int   idx = tl->currentClipIndex();
        if (!trk || idx < 0 || idx >= trk->clipCount()) {
            m_announcer->announce(tr("No clip selected."),
                                  Announcer::Priority::Normal);
            return;
        }
        auto *src = trk->clipAt(idx);
        delete m_clipboardClip;
        m_clipboardClip = new Clip(src->name(), src->sourcePath(),
                                   src->inPoint(), src->outPoint(), this);
        m_clipboardClip->setTimelinePosition(src->timelinePosition());
        for (auto *fx : src->effects())
            m_clipboardClip->addEffect(
                new Effect(fx->serviceId(), fx->displayName(),
                           fx->description(), m_clipboardClip));

        m_announcer->announce(
            tr("Copied: %1").arg(src->name()), Announcer::Priority::Normal);
    });

    connect(m_actPaste, &QAction::triggered, this, [this]() {
        if (!m_clipboardClip) {
            m_announcer->announce(tr("Clipboard is empty."),
                                  Announcer::Priority::Normal);
            return;
        }
        auto *tl  = m_project->timeline();
        auto *trk = tl->trackAt(tl->currentTrackIndex());
        if (!trk) {
            m_announcer->announce(tr("No track available."),
                                  Announcer::Priority::Normal);
            return;
        }
        // Create a new clip from the clipboard data
        auto *newClip = new Clip(m_clipboardClip->name(),
                                 m_clipboardClip->sourcePath(),
                                 m_clipboardClip->inPoint(),
                                 m_clipboardClip->outPoint());
        newClip->setTimelinePosition(tl->playheadPosition());
        for (auto *fx : m_clipboardClip->effects())
            newClip->addEffect(
                new Effect(fx->serviceId(), fx->displayName(),
                           fx->description(), newClip));

        m_undoStack->push(new AddClipCommand(trk, newClip));
        m_modified = true;
        m_announcer->announce(
            tr("Pasted: %1").arg(newClip->name()),
            Announcer::Priority::Normal);
    });

    connect(m_actDelete, &QAction::triggered, this, [this]() {
        auto *tl  = m_project->timeline();
        auto *trk = tl->trackAt(tl->currentTrackIndex());
        int   idx = tl->currentClipIndex();
        if (!trk || idx < 0 || idx >= trk->clipCount()) {
            m_announcer->announce(tr("No clip selected."),
                                  Announcer::Priority::Normal);
            return;
        }
        const QString name = trk->clipAt(idx)->name();
        m_undoStack->push(new RemoveClipCommand(trk, idx));
        m_modified = true;
        m_announcer->announce(
            tr("Deleted: %1").arg(name), Announcer::Priority::Normal);
    });

    connect(m_actSelectAll, &QAction::triggered, this, [this]() {
        auto *tl = m_project->timeline();
        m_announcer->announce(
            tr("%n track(s), %1 total clips.", nullptr, tl->trackCount())
                .arg([tl]() {
                    int total = 0;
                    for (auto *t : tl->tracks()) total += t->clipCount();
                    return total;
                }()),
            Announcer::Priority::Normal);
    });

    // ── Timeline operations ───────────────────────────────────────
    m_actSplitClip = new QAction(tr("&Split Clip at Playhead"), this);
    m_actSplitClip->setShortcut(QKeySequence(Qt::Key_S));
    connect(m_actSplitClip, &QAction::triggered, this, [this]() {
        auto *tl  = m_project->timeline();
        auto *trk = tl->trackAt(tl->currentTrackIndex());
        int   idx = tl->currentClipIndex();
        if (!trk || idx < 0 || idx >= trk->clipCount()) {
            m_announcer->announce(tr("No clip selected to split."),
                                  Announcer::Priority::Normal);
            return;
        }
        m_undoStack->push(new SplitClipCommand(trk, idx, tl->playheadPosition()));
        m_announcer->announce(tr("Clip split at playhead."),
                              Announcer::Priority::High);
    });

    m_actAddTrack = new QAction(tr("Add &Track…"), this);
    m_actAddTrack->setShortcut(QKeySequence(Qt::Key_T));
    connect(m_actAddTrack, &QAction::triggered, this, [this]() {
        QStringList types;
        types << tr("Video") << tr("Audio");
        bool ok = false;
        const QString chosen = QInputDialog::getItem(
            this, tr("Add Track"), tr("Track type:"),
            types, 0, false, &ok);
        if (!ok) return;

        auto type = (chosen == tr("Video")) ? Track::Type::Video
                                            : Track::Type::Audio;
        auto *tl = m_project->timeline();
        int num = tl->trackCount() + 1;
        auto *track = new Track(
            tr("%1 %2").arg(chosen).arg(num), type);
        m_undoStack->push(new AddTrackCommand(tl, track));
        m_announcer->announce(
            tr("Added %1.").arg(track->name()),
            Announcer::Priority::High);
    });

    m_actRemoveTrack = new QAction(tr("&Remove Current Track"), this);
    m_actRemoveTrack->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Delete));
    connect(m_actRemoveTrack, &QAction::triggered, this, [this]() {
        auto *tl = m_project->timeline();
        int idx = tl->currentTrackIndex();
        if (idx < 0 || idx >= tl->trackCount()) {
            m_announcer->announce(tr("No track to remove."),
                                  Announcer::Priority::Normal);
            return;
        }
        const QString name = tl->trackAt(idx)->name();
        m_undoStack->push(new RemoveTrackCommand(tl, idx));
        m_announcer->announce(
            tr("Removed %1.").arg(name), Announcer::Priority::High);
    });

    m_actAddMarker = new QAction(tr("Add &Marker at Playhead…"), this);
    m_actAddMarker->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_M));
    connect(m_actAddMarker, &QAction::triggered, this, [this]() {
        bool ok = false;
        const QString name = QInputDialog::getText(
            this, tr("Add Marker"), tr("Marker name:"),
            QLineEdit::Normal, tr("Marker"), &ok);
        if (!ok || name.isEmpty()) return;

        auto *tl = m_project->timeline();
        m_undoStack->push(new AddMarkerCommand(
            tl, name, tl->playheadPosition()));
        m_announcer->announce(
            tr("Marker \"%1\" added at %2.")
                .arg(name, tl->playheadPosition().toString()),
            Announcer::Priority::High);
    });

    // ── Add Transition ──────────────────────────────────────────
    m_actAddTransition = new QAction(tr("Add &Transition to Clip…"), this);
    connect(m_actAddTransition, &QAction::triggered, this, [this]() {
        auto *tl  = m_project->timeline();
        auto *trk = tl->trackAt(tl->currentTrackIndex());
        int   idx = tl->currentClipIndex();
        if (!trk || idx < 0 || idx >= trk->clipCount()) {
            m_announcer->announce(tr("Select a clip first."),
                                  Announcer::Priority::Normal);
            return;
        }

        QStringList types;
        types << tr("luma (Cross Dissolve)")
              << tr("mix (Audio Cross-fade)");
        bool ok = false;
        const QString chosen = QInputDialog::getItem(
            this, tr("Add Transition"), tr("Transition type:"),
            types, 0, false, &ok);
        if (!ok) return;

        bool durationOk = false;
        const double seconds = QInputDialog::getDouble(
            this, tr("Transition Duration"),
            tr("Duration in seconds:"), 1.0, 0.1, 30.0, 1, &durationOk);
        if (!durationOk) return;

        const QString serviceId = chosen.startsWith(tr("luma"))
                                      ? QStringLiteral("luma")
                                      : QStringLiteral("mix");
        const int frames = static_cast<int>(seconds * m_project->fps());
        auto *trans = new Transition(
            serviceId,
            chosen.section(QLatin1Char('('), 1).chopped(1).trimmed(),
            QString(),
            TimeCode(frames, m_project->fps()));

        auto *clip = trk->clipAt(idx);
        m_undoStack->push(new AddTransitionCommand(
            clip, AddTransitionCommand::Edge::Out, trans));
        rebuildTractor();
        m_announcer->announce(
            tr("Added %1 transition (%2s) to %3.")
                .arg(serviceId)
                .arg(seconds, 0, 'f', 1)
                .arg(clip->name()),
            Announcer::Priority::High);
    });

    connect(m_actNew,   &QAction::triggered, this, &MainWindow::newProject);
    connect(m_actOpen,  &QAction::triggered, this, &MainWindow::openProject);
    connect(m_actSave,  &QAction::triggered, this, &MainWindow::saveProject);
    connect(m_actSaveAs, &QAction::triggered, this, &MainWindow::saveProjectAs);
    connect(m_actExport, &QAction::triggered, this, &MainWindow::exportVideo);
    connect(m_actPreferences, &QAction::triggered, this, &MainWindow::showPreferences);
    connect(m_actWizard, &QAction::triggered, this, &MainWindow::showWelcomeWizard);
    connect(m_actAbout, &QAction::triggered, this, &MainWindow::showAbout);
    connect(m_actQuit,  &QAction::triggered, qApp, &QApplication::quit);
}

void MainWindow::createMenus()
{
    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_actNew);
    fileMenu->addAction(m_actOpen);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actSave);
    fileMenu->addAction(m_actSaveAs);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actExport);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actPreferences);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actQuit);

    auto *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(m_actUndo);
    editMenu->addAction(m_actRedo);
    editMenu->addSeparator();
    editMenu->addAction(m_actCut);
    editMenu->addAction(m_actCopy);
    editMenu->addAction(m_actPaste);
    editMenu->addAction(m_actDelete);
    editMenu->addSeparator();
    editMenu->addAction(m_actSelectAll);

    auto *timelineMenu = menuBar()->addMenu(tr("Time&line"));
    timelineMenu->addAction(m_actSplitClip);
    timelineMenu->addSeparator();
    timelineMenu->addAction(m_actAddTrack);
    timelineMenu->addAction(m_actRemoveTrack);
    timelineMenu->addSeparator();
    timelineMenu->addAction(m_actAddMarker);
    timelineMenu->addSeparator();
    timelineMenu->addAction(m_actAddTransition);

    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(m_actWizard);
    helpMenu->addAction(m_actAbout);
}

void MainWindow::createDockWidgets()
{
    // Central widget: Timeline
    m_timeline = new TimelineWidget(
        m_project->timeline(), m_announcer, m_cues, this);
    setCentralWidget(m_timeline);

    // Transport bar (bottom)
    m_transport = new TransportBar(
        m_playback, m_project->timeline(), m_announcer, this);
    auto *transportDock = new QDockWidget(tr("Transport"), this);
    transportDock->setWidget(m_transport);
    transportDock->setFeatures(QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::BottomDockWidgetArea, transportDock);

    // Media browser (left)
    m_media = new MediaBrowser(m_announcer, this);
    auto *mediaDock = new QDockWidget(tr("Media"), this);
    mediaDock->setWidget(m_media);
    addDockWidget(Qt::LeftDockWidgetArea, mediaDock);

    // Properties panel (right)
    m_properties = new PropertiesPanel(m_announcer, m_undoStack, this);
    auto *propDock = new QDockWidget(tr("Properties"), this);
    propDock->setWidget(m_properties);
    addDockWidget(Qt::RightDockWidgetArea, propDock);

    // Rebuild tractor when a clip is trimmed from the properties panel
    connect(m_properties, &PropertiesPanel::clipTrimmed,
            this, &MainWindow::rebuildTractor);

    // Effects browser (right, tabbed with properties)
    m_effects = new EffectsBrowser(m_catalog, m_announcer, this);
    auto *effectsDock = new QDockWidget(tr("Effects"), this);
    effectsDock->setWidget(m_effects);
    tabifyDockWidget(propDock, effectsDock);

    // Wire timeline selection → properties
    connect(m_timeline, &TimelineWidget::focusedClipChanged,
            this, [this]() {
                auto *tl  = m_project->timeline();
                auto *trk = tl->trackAt(tl->currentTrackIndex());
                if (!trk) { m_properties->clear(); return; }
                int idx = tl->currentClipIndex();
                if (idx >= 0 && idx < trk->clips().size()) {
                    m_properties->inspectClip(trk->clips().at(idx));
                } else {
                    m_properties->inspectTrack(trk);
                }
            });

    // ── Media import → timeline ──────────────────────────────────────
    connect(m_media, &MediaBrowser::fileActivated,
            this, [this](const QString &filePath) {
                auto *tl = m_project->timeline();

                // Auto-create default video + audio tracks if the timeline
                // is empty (first import).
                if (tl->trackCount() == 0) {
                    m_undoStack->push(
                        new AddTrackCommand(
                            tl,
                            new Track(tr("Video 1"), Track::Type::Video)));
                    m_undoStack->push(
                        new AddTrackCommand(
                            tl,
                            new Track(tr("Audio 1"), Track::Type::Audio)));
                }

                // Probe the file with MLT to get its duration
                auto *profile = m_engine->compositionProfile();
                Mlt::Producer probe(*profile, filePath.toUtf8().constData());
                if (!probe.is_valid()) {
                    m_announcer->announce(
                        tr("Cannot open file: %1").arg(QFileInfo(filePath).fileName()),
                        Announcer::Priority::High);
                    return;
                }

                const double fps = m_project->fps();
                const int length = probe.get_length();
                TimeCode inPt(0, fps);
                TimeCode outPt(length > 0 ? length - 1 : 0, fps);

                // Place the clip at the playhead position
                auto *clip = new Clip(
                    QFileInfo(filePath).fileName(),
                    filePath, inPt, outPt);
                clip->setTimelinePosition(tl->playheadPosition());

                // Add to the first video track (index 0)
                auto *trk = tl->trackAt(0);
                if (!trk) return;

                m_undoStack->push(new AddClipCommand(trk, clip));
                m_modified = true;
                m_timeline->refresh();

                m_announcer->announce(
                    tr("Added %1 to %2 (%3).")
                        .arg(clip->name(), trk->name(),
                             clip->duration().toSpokenString()),
                    Announcer::Priority::High);
            });

    // ── Effects browser → apply to selected clip ─────────────────────
    connect(m_effects, &EffectsBrowser::effectChosen,
            this, [this](const QString &serviceId) {
                auto *tl  = m_project->timeline();
                auto *trk = tl->trackAt(tl->currentTrackIndex());
                int   idx = tl->currentClipIndex();
                if (!trk || idx < 0 || idx >= trk->clipCount()) {
                    m_announcer->announce(
                        tr("Select a clip first before applying an effect."),
                        Announcer::Priority::High);
                    return;
                }

                auto *clip   = trk->clipAt(idx);
                auto *effect = new Effect(serviceId, serviceId, QString());
                m_undoStack->push(new AddEffectCommand(clip, effect));
                m_modified = true;

                m_announcer->announce(
                    tr("Applied effect %1 to %2.")
                        .arg(serviceId, clip->name()),
                    Announcer::Priority::High);
            });
}

void MainWindow::registerShortcuts()
{
    auto &sm = ShortcutManager::instance();

    sm.registerAction(QStringLiteral("file.new"),   m_actNew,
                      QKeySequence(QStringLiteral("Ctrl+N")));
    sm.registerAction(QStringLiteral("file.open"),  m_actOpen,
                      QKeySequence(QStringLiteral("Ctrl+O")));
    sm.registerAction(QStringLiteral("file.save"),  m_actSave,
                      QKeySequence(QStringLiteral("Ctrl+S")));
    sm.registerAction(QStringLiteral("file.saveAs"), m_actSaveAs,
                      QKeySequence(QStringLiteral("Ctrl+Shift+S")));
    sm.registerAction(QStringLiteral("file.export"), m_actExport,
                      QKeySequence(QStringLiteral("Ctrl+Shift+E")));
    sm.registerAction(QStringLiteral("edit.undo"),  m_actUndo,
                      QKeySequence(QStringLiteral("Ctrl+Z")));
    sm.registerAction(QStringLiteral("edit.redo"),  m_actRedo,
                      QKeySequence(QStringLiteral("Ctrl+Y")));
    sm.registerAction(QStringLiteral("edit.preferences"), m_actPreferences,
                      QKeySequence(QStringLiteral("Ctrl+,")));
    sm.registerAction(QStringLiteral("help.wizard"), m_actWizard,
                      QKeySequence());
    sm.registerAction(QStringLiteral("help.about"), m_actAbout,
                      QKeySequence());
    sm.registerAction(QStringLiteral("app.quit"),   m_actQuit,
                      QKeySequence(QStringLiteral("Ctrl+Q")));

    // Timeline menu shortcuts
    sm.registerAction(QStringLiteral("timeline.split"), m_actSplitClip,
                      QKeySequence(QStringLiteral("S")));
    sm.registerAction(QStringLiteral("timeline.addTrack"), m_actAddTrack,
                      QKeySequence(QStringLiteral("T")));
    sm.registerAction(QStringLiteral("timeline.removeTrack"), m_actRemoveTrack,
                      QKeySequence(QStringLiteral("Shift+Delete")));
    sm.registerAction(QStringLiteral("timeline.addMarker"), m_actAddMarker,
                      QKeySequence(QStringLiteral("Shift+M")));
    sm.registerAction(QStringLiteral("timeline.addTransition"), m_actAddTransition,
                      QKeySequence(QStringLiteral("Shift+T")));
}

void MainWindow::rebuildTractor()
{
    m_tractorBuilder->rebuild(m_project->timeline());
    m_timeline->refresh();
}

void MainWindow::reconnectTimeline()
{
    auto *tl = m_project->timeline();

    // Reconnect the tracksChanged signal to rebuild the MLT pipeline
    connect(tl, &Timeline::tracksChanged,
            this, &MainWindow::rebuildTractor);

    // Update the TimelineWidget and TransportBar to use the new Timeline
    m_timeline->setTimeline(tl);
    m_transport->setTimeline(tl);

    // Rebuild the tractor for the new timeline state
    rebuildTractor();
}

void MainWindow::updateWindowTitle()
{
    QString title = m_project->name();
    if (m_modified)
        title.prepend(QLatin1String("* "));
    title.append(tr(" — Thrive Video Suite"));
    setWindowTitle(title);
}

void MainWindow::checkFirstRun()
{
    QSettings settings;
    const bool firstRun = settings.value(
        QLatin1String(kSettingsFirstRun), true).toBool();

    if (firstRun) {
        settings.setValue(QLatin1String(kSettingsFirstRun), false);
        showWelcomeWizard();
    }
}

void MainWindow::checkPostRestartPlugins()
{
    QSettings settings;
    if (settings.value(QLatin1String(kSettingsPluginJustInstalled), false)
            .toBool()) {
        settings.remove(QLatin1String(kSettingsPluginJustInstalled));
        m_announcer->announce(
            tr("Plugins updated. Changes are now active."),
            Announcer::Priority::High);
    }
}

} // namespace Thrive
