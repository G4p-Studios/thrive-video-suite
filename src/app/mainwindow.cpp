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
#include "../accessibility/screenreader.h"

#include "../ui/timelinewidget.h"
#include "../ui/transportbar.h"
#include "../ui/mediabrowser.h"
#include "../ui/propertiespanel.h"
#include "../ui/effectsbrowser.h"
#include "../ui/videopreviewwidget.h"
#include "../ui/preferencesdialog.h"
#include "../ui/shortcuteditor.h"
#include "../ui/exportdialog.h"
#include "../ui/exportprogressdialog.h"

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
#include <QLineEdit>
#include <QMessageBox>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QSettings>
#include <QAbstractItemView>
#include <QApplication>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QAbstractButton>
#include <QComboBox>
#include <QAbstractSpinBox>
#include <QSignalBlocker>
#include <QSplitter>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QFont>
#include <QFile>
#include <QUuid>

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

    // Debounce timer: coalesces rapid rebuildTractor calls so the
    // consumer is not torn down / re-created many times per second.
    m_rebuildTimer = new QTimer(this);
    m_rebuildTimer->setSingleShot(true);
    m_rebuildTimer->setInterval(100);          // 100 ms debounce
    connect(m_rebuildTimer, &QTimer::timeout,
            this, &MainWindow::rebuildTractor);

    // Rebuild the MLT pipeline whenever the timeline model changes
    connect(m_project->timeline(), &Timeline::tracksChanged,
            this, &MainWindow::deferRebuildTractor);

    // Safely handle timeline replacement (e.g. Project::reset)
    connect(m_project, &Project::timelineAboutToChange,
            this, [this]() {
                // Disconnect from the old timeline before it is deleted
                m_project->timeline()->disconnect(this);
            });

    // When a new tractor is ready, update the producer reference.
    // The consumer is NOT started here — only an explicit Play command
    // (or step/seek) will open it.  This avoids the crash caused by
    // rapid close/open cycles when clips are added in quick succession.
    connect(m_tractorBuilder, &TractorBuilder::tractorReady,
            this, [this](Mlt::Tractor *tractor) {
                const bool wasPlaying =
                    m_playback->state() == PlaybackController::State::Playing;
                m_playback->close();
                m_playback->setProducer(tractor);
                if (wasPlaying) {
                    m_playback->open();
                    m_playback->play();
                }
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

    // Load persisted accessibility/navigation preferences
    {
        QSettings settings;
        const int verbosity = settings.value(
            QLatin1String(kSettingsContextVerbosity), 1).toInt();
        switch (verbosity) {
        case 0:
            m_contextVerbosity = ContextVerbosity::Short;
            break;
        case 2:
            m_contextVerbosity = ContextVerbosity::Detailed;
            break;
        default:
            m_contextVerbosity = ContextVerbosity::Normal;
            break;
        }

        const bool markerSnap = settings.value(
            QLatin1String(kSettingsMarkerJumpSnap), true).toBool();
        if (m_actToggleMarkerJumpSnap) {
            const QSignalBlocker blocker(m_actToggleMarkerJumpSnap);
            m_actToggleMarkerJumpSnap->setChecked(markerSnap);
        }

        const int dryRunMode = settings.value(
            QLatin1String(kSettingsIntroDryRunMode), 0).toInt();
        switch (dryRunMode) {
        case 1:
            m_introDryRunMode = IntroDryRunMode::VisualOnly;
            break;
        case 2:
            m_introDryRunMode = IntroDryRunMode::AnnouncementOnly;
            break;
        default:
            m_introDryRunMode = IntroDryRunMode::AutoDetect;
            break;
        }
    }

    createMenus();
    createDockWidgets();
    registerShortcuts();
    setupAutoSave();

    // Load saved shortcuts
    ShortcutManager::instance().load();

    // Post-restart plugin announcement
    checkPostRestartPlugins();

    // First-run wizard
    checkFirstRun();

    // Check for auto-save recovery
    checkAutoSaveRecovery();

    // Restore window geometry and dock layout from previous session.
    // Deferred to a zero-timer so the restore runs after the initial
    // show() layout pass – avoids crashes from incompatible state blobs.
    QTimer::singleShot(0, this, [this]() {
        QSettings geo;
        const QByteArray geom =
            geo.value(QStringLiteral("mainwindow/geometry")).toByteArray();
        const QByteArray state =
            geo.value(QStringLiteral("mainwindow/state")).toByteArray();
        if (!geom.isEmpty())
            restoreGeometry(geom);
        if (!state.isEmpty()) {
            if (!restoreState(state, kWindowStateVersion)) {
                // Discard incompatible state
                geo.remove(QStringLiteral("mainwindow/state"));
            }
        }
    });
}

// ── close ────────────────────────────────────────────────────────────

// ---------------------------------------------------------------------------
// Allow interactive widgets (buttons, combos, spin-boxes, check-boxes, etc.)
// to handle Space / Enter / Return normally, even though those keys are
// registered as global shortcuts (e.g. Play/Pause on Space).
// Qt sends ShortcutOverride to the focused widget *before* matching a
// shortcut.  If we accept the event here, the key press goes to the widget
// instead of firing the shortcut.
// ---------------------------------------------------------------------------
bool MainWindow::event(QEvent *ev)
{
    if (ev->type() == QEvent::ShortcutOverride) {
        auto *ke = static_cast<QKeyEvent *>(ev);
        const int key = ke->key();
        if (key == Qt::Key_Space || key == Qt::Key_Return ||
            key == Qt::Key_Enter) {
            QWidget *fw = QApplication::focusWidget();
            if (fw && (qobject_cast<QAbstractButton *>(fw) ||
                       qobject_cast<QComboBox *>(fw) ||
                       qobject_cast<QAbstractSpinBox *>(fw) ||
                       qobject_cast<QLineEdit *>(fw) ||
                       qobject_cast<QAbstractItemView *>(fw))) {
                ev->accept();   // let the widget handle the key
                return true;
            }
        }
    }
    return QMainWindow::event(ev);
}

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

    // Remove auto-save file on clean exit
    removeAutoSaveFile();

    // Persist window geometry and dock layout
    QSettings s;
    s.setValue(QStringLiteral("mainwindow/geometry"), saveGeometry());
    s.setValue(QStringLiteral("mainwindow/state"),
               saveState(kWindowStateVersion));

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

    // Remove stale auto-save
    removeAutoSaveFile();

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
    addRecentProject(path);
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
    addRecentProject(m_currentFilePath);

    // Remove auto-save file after successful explicit save
    removeAutoSaveFile();

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
    auto *tractor = m_tractorBuilder->tractor();
    if (!tractor) {
        m_announcer->announce(
            tr("Nothing to export. Add clips to the timeline first."),
            Announcer::Priority::High);
        return;
    }

    // Show export settings dialog
    auto *dlg = new ExportDialog(this);
    if (dlg->exec() != QDialog::Accepted || dlg->outputPath().isEmpty()) {
        dlg->deleteLater();
        return;
    }

    const QString path   = dlg->outputPath();
    const QString format = dlg->format();
    const QString vcodec = dlg->videoCodec();
    const QString acodec = dlg->audioCodec();
    const int vBitrate   = dlg->videoBitrate();
    const int aBitrate   = dlg->audioBitrate();
    dlg->deleteLater();

    // Stop playback entirely before rendering – having the sdl2_audio
    // consumer still running while the avformat render consumer is active
    // can cause thread-safety issues in MLT.
    m_playback->close();

    // Freeze the debounce timer so that no tractor-rebuild occurs while
    // the render engine is cloning / encoding the tractor.
    m_rebuildTimer->stop();

    // Disconnect previous render connections
    disconnect(m_render, &RenderEngine::renderProgress, this, nullptr);
    disconnect(m_render, &RenderEngine::renderFinished, this, nullptr);

    m_announcer->announce(tr("Export started…"), Announcer::Priority::High);

    if (!m_render->startRender(tractor, path, format, vcodec, acodec,
                               vBitrate, aBitrate)) {
        m_announcer->announce(tr("Export failed to start."),
                              Announcer::Priority::High);
        return;
    }

    // Show modal progress dialog
    auto *progress = new ExportProgressDialog(m_render, m_announcer, this);
    progress->exec();
    progress->deleteLater();

    // Re-enable the rebuild timer now that export is done
    // (it won't fire until something actually calls deferRebuildTractor)
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
    connect(dlg, &PreferencesDialog::contextVerbosityChanged,
            this, [this](int verbosity) {
                switch (verbosity) {
                case 0:
                    m_contextVerbosity = ContextVerbosity::Short;
                    break;
                case 2:
                    m_contextVerbosity = ContextVerbosity::Detailed;
                    break;
                default:
                    m_contextVerbosity = ContextVerbosity::Normal;
                    break;
                }
            });
    connect(dlg, &PreferencesDialog::markerJumpSnapChanged,
            this, [this](bool enabled) {
                if (m_actToggleMarkerJumpSnap)
                    m_actToggleMarkerJumpSnap->setChecked(enabled);
                if (m_timeline)
                    m_timeline->setMarkerJumpSnapEnabled(enabled);
            });
    connect(dlg, &PreferencesDialog::introDryRunModeChanged,
            this, [this](int mode) {
                switch (mode) {
                case 1:
                    m_introDryRunMode = IntroDryRunMode::VisualOnly;
                    break;
                case 2:
                    m_introDryRunMode = IntroDryRunMode::AnnouncementOnly;
                    break;
                default:
                    m_introDryRunMode = IntroDryRunMode::AutoDetect;
                    break;
                }
            });
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
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }
        if (trk->isLocked()) {
            m_announcer->announce(tr("Track is locked."),
                                  Announcer::Priority::Normal);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }
        // Deep-copy to clipboard before removing
        auto *src = trk->clipAt(idx);
        const QString clipName = src->name();
        delete m_clipboardClip;
        m_clipboardClip = Clip::deepCopy(src, this);

        m_undoStack->push(new RemoveClipCommand(trk, idx));
        m_modified = true;
        deferRebuildTractor();
        m_cues->play(AudioCueManager::Cue::ClipRemoved);
        m_announcer->announce(
            tr("Cut: %1").arg(clipName),
            Announcer::Priority::Normal);
    });

    connect(m_actCopy, &QAction::triggered, this, [this]() {
        auto *tl  = m_project->timeline();
        auto *trk = tl->trackAt(tl->currentTrackIndex());
        int   idx = tl->currentClipIndex();
        if (!trk || idx < 0 || idx >= trk->clipCount()) {
            m_announcer->announce(tr("No clip selected."),
                                  Announcer::Priority::Normal);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }
        auto *src = trk->clipAt(idx);
        delete m_clipboardClip;
        m_clipboardClip = Clip::deepCopy(src, this);

        m_announcer->announce(
            tr("Copied: %1").arg(src->name()), Announcer::Priority::Normal);
    });

    connect(m_actPaste, &QAction::triggered, this, [this]() {
        if (!m_clipboardClip) {
            m_announcer->announce(tr("Clipboard is empty."),
                                  Announcer::Priority::Normal);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }
        auto *tl  = m_project->timeline();
        auto *trk = tl->trackAt(tl->currentTrackIndex());
        if (!trk) {
            m_announcer->announce(tr("No track available."),
                                  Announcer::Priority::Normal);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }
        if (trk->isLocked()) {
            m_announcer->announce(tr("Track is locked."),
                                  Announcer::Priority::Normal);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }
        // Create a new clip from the clipboard data (deep copy)
        auto *newClip = Clip::deepCopy(m_clipboardClip);
        newClip->setTimelinePosition(tl->playheadPosition());

        m_undoStack->push(new AddClipCommand(trk, newClip));
        m_modified = true;
        rebuildTractor();
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
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }
        if (trk->isLocked()) {
            m_announcer->announce(tr("Track is locked."),
                                  Announcer::Priority::Normal);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }
        const QString name = trk->clipAt(idx)->name();
        m_undoStack->push(new RemoveClipCommand(trk, idx));
        m_modified = true;
        deferRebuildTractor();
        m_cues->play(AudioCueManager::Cue::ClipRemoved);
        m_announcer->announce(
            tr("Removed %1 from %2.").arg(name, trk->name()),
            Announcer::Priority::High);
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
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }
        if (trk->isLocked()) {
            m_announcer->announce(tr("Track is locked."),
                                  Announcer::Priority::Normal);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }
        m_undoStack->push(new SplitClipCommand(trk, idx, tl->playheadPosition()));
        rebuildTractor();
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

    m_actBuildIntroStack = new QAction(tr("Build &Intro Stack…"), this);
    connect(m_actBuildIntroStack, &QAction::triggered, this, [this]() {
        auto *tl = m_project->timeline();
        if (!tl) {
            m_announcer->announce(tr("Timeline is not available."),
                                  Announcer::Priority::High);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }

        const QString visualFilter =
            tr("Visual Media (*.png *.jpg *.jpeg *.bmp *.webp *.gif *.mp4 *.mov *.mkv *.avi);;All Files (*)");

        const QString ringsPath = QFileDialog::getOpenFileName(
            this, tr("Select Rings Background"), QString(), visualFilter);
        if (ringsPath.isEmpty())
            return;

        const QString shieldPath = QFileDialog::getOpenFileName(
            this, tr("Select Shield Overlay"), QString(), visualFilter);
        if (shieldPath.isEmpty())
            return;

        bool captionOk = false;
        const QString captionText = QInputDialog::getText(
            this,
            tr("Caption Text"),
            tr("Caption text above shield (leave empty to skip):"),
            QLineEdit::Normal,
            tr("WARNER BROS."),
            &captionOk);
        if (!captionOk)
            return;

        const bool includeLooney =
            QMessageBox::question(
                this,
                tr("Include Looney Text"),
                tr("Add Looney text phase after shield and caption fade out?"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes) == QMessageBox::Yes;

        QString looneyText;
        if (includeLooney) {
            bool looneyOk = false;
            looneyText = QInputDialog::getText(
                this,
                tr("Looney Text"),
                tr("Looney text content:"),
                QLineEdit::Normal,
                tr("LOONEY TUNES"),
                &looneyOk);
            if (!looneyOk)
                return;
        }

        bool ok = false;
        const double totalSeconds = QInputDialog::getDouble(
            this, tr("Intro Duration"),
            tr("Total intro length in seconds:"),
            6.0, 1.0, 60.0, 1, &ok);
        if (!ok)
            return;

        const double shieldInSeconds = QInputDialog::getDouble(
            this, tr("Shield In Time"),
            tr("Shield start time in seconds:"),
            0.6, 0.0, totalSeconds, 1, &ok);
        if (!ok)
            return;

        const double captionInSeconds = QInputDialog::getDouble(
            this, tr("Caption In Time"),
            tr("Caption start time in seconds:"),
            1.2, 0.0, totalSeconds, 1, &ok);
        if (!ok)
            return;

        double replaceSeconds = totalSeconds;
        if (includeLooney) {
            replaceSeconds = QInputDialog::getDouble(
                this, tr("Looney Text Start"),
                tr("Looney text start time in seconds:"),
                3.5, 0.0, totalSeconds, 1, &ok);
            if (!ok)
                return;
        }

        const double fadeSeconds = QInputDialog::getDouble(
            this, tr("Fade Duration"),
            tr("Fade duration in seconds:"),
            0.8, 0.1, 10.0, 1, &ok);
        if (!ok)
            return;

        const double fps = m_project->fps();
        const int64_t baseStart = tl->playheadPosition().frame();
        const int64_t totalFrames = qMax<int64_t>(1, static_cast<int64_t>(totalSeconds * fps));
        const int64_t shieldStart = qMax<int64_t>(0, static_cast<int64_t>(shieldInSeconds * fps));
        const int64_t captionStart = qMax<int64_t>(0, static_cast<int64_t>(captionInSeconds * fps));
        const int64_t replaceStart = qMax<int64_t>(0, static_cast<int64_t>(replaceSeconds * fps));
        const int64_t fadeFrames = qMax<int64_t>(1, static_cast<int64_t>(fadeSeconds * fps));

        const QString dryRunSummary = tr(
            "Dry run summary. "
            "Start at %1. Total %2 seconds. "
            "Shield starts at %3 seconds. "
            "Caption starts at %4 seconds. "
            "Fade duration %5 seconds. "
            "Looney text phase %6.")
            .arg(TimeCode(baseStart, fps).toSpokenString())
            .arg(totalSeconds, 0, 'f', 1)
            .arg(shieldInSeconds, 0, 'f', 1)
            .arg(captionInSeconds, 0, 'f', 1)
            .arg(fadeSeconds, 0, 'f', 1)
            .arg(includeLooney
                ? tr("enabled at %1 seconds").arg(replaceSeconds, 0, 'f', 1)
                : tr("disabled"));

        const bool screenReaderActive = ScreenReader::instance().isScreenReaderActive();
        const bool announcementOnly =
            m_introDryRunMode == IntroDryRunMode::AnnouncementOnly
            || (m_introDryRunMode == IntroDryRunMode::AutoDetect
                && screenReaderActive);

        if (announcementOnly) {
            m_announcer->announce(dryRunSummary, Announcer::Priority::High);
            m_announcer->announce(tr("Creating intro stack now."),
                                  Announcer::Priority::Normal);
        } else {
            QMessageBox::information(
                this,
                tr("Intro Stack Dry Run"),
                dryRunSummary);
            const auto answer = QMessageBox::question(
                this,
                tr("Build Intro Stack"),
                tr("Apply this intro stack now?"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes);
            if (answer != QMessageBox::Yes)
                return;
        }

        auto ensureVideoTrack = [this, tl](const QString &baseName) -> Track * {
            auto *track = new Track(baseName, Track::Type::Video);
            m_undoStack->push(new AddTrackCommand(tl, track));
            return track;
        };

        auto makeTextImage = [this](const QString &text) -> QString {
            if (text.trimmed().isEmpty())
                return QString();
            const int width = qMax(320, m_project->width());
            const int height = qMax(180, m_project->height());
            QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);

            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setRenderHint(QPainter::TextAntialiasing, true);

            QFont font;
            font.setPointSize(qMax(24, height / 12));
            font.setBold(true);
            painter.setFont(font);

            QPainterPath path;
            path.addText(0.0, 0.0, font, text);
            const QRectF b = path.boundingRect();
            QTransform transform;
            transform.translate((width - b.width()) / 2.0 - b.left(),
                                (height - b.height()) / 2.0 - b.top());
            QPainterPath centered = transform.map(path);

            painter.setPen(QPen(QColor(0, 0, 0, 220), 4.0,
                                Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(QColor(255, 255, 255, 255));
            painter.drawPath(centered);
            painter.end();

            const QString baseDir = QStandardPaths::writableLocation(
                QStandardPaths::AppDataLocation) + QStringLiteral("/text-overlays");
            QDir().mkpath(baseDir);
            const QString filePath = baseDir
                + QStringLiteral("/text_overlay_")
                + QUuid::createUuid().toString(QUuid::WithoutBraces)
                + QStringLiteral(".png");
            if (!image.save(filePath, "PNG"))
                return QString();
            return filePath;
        };

        auto addClip = [this, fps, baseStart](Track *track,
                                              const QString &name,
                                              const QString &path,
                                              int64_t relStart,
                                              int64_t durationFrames) -> Clip * {
            if (!track || path.isEmpty() || durationFrames <= 0)
                return nullptr;
            auto *clip = new Clip(name,
                                  path,
                                  TimeCode(0, fps),
                                  TimeCode(durationFrames - 1, fps));
            clip->setTimelinePosition(TimeCode(baseStart + relStart, fps));
            m_undoStack->push(new AddClipCommand(track, clip));
            return clip;
        };

        auto ensureEffect = [this](Clip *clip,
                                   const QString &sid,
                                   const QString &name,
                                   const QString &desc) -> Effect * {
            if (!clip)
                return nullptr;
            for (auto *fx : clip->effects()) {
                if (fx && fx->serviceId() == sid)
                    return fx;
            }
            auto *fx = new Effect(sid, name, desc);
            m_undoStack->push(new AddEffectCommand(clip, fx));
            return fx;
        };

        auto ensureStringParam = [](Effect *fx, const QString &id,
                                    const QString &display,
                                    const QString &value) {
            if (!fx)
                return;
            bool found = false;
            for (const auto &p : fx->parameters()) {
                if (p.id == id) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                EffectParameter param;
                param.id = id;
                param.displayName = display;
                param.type = QStringLiteral("string");
                param.defaultValue = value;
                param.currentValue = value;
                fx->addParameter(param);
            }
            fx->setParameterValue(id, value);
        };

        m_undoStack->beginMacro(tr("Build intro stack"));

        auto *ringsTrack = ensureVideoTrack(tr("Rings"));
        auto *shieldTrack = ensureVideoTrack(tr("Shield"));
        auto *captionTrack = captionText.trimmed().isEmpty()
            ? nullptr : ensureVideoTrack(tr("Caption"));
        auto *looneyTrack = includeLooney
            ? ensureVideoTrack(tr("Looney Text")) : nullptr;

        auto *ringsClip = addClip(ringsTrack, tr("Rings"), ringsPath,
                                  0, totalFrames);
        Q_UNUSED(ringsClip)

        int64_t shieldDuration = includeLooney
            ? qMax<int64_t>(1, (replaceStart + fadeFrames) - shieldStart)
            : qMax<int64_t>(1, totalFrames - shieldStart);
        auto *shieldClip = addClip(shieldTrack, tr("Shield"), shieldPath,
                                   shieldStart, shieldDuration);

        if (shieldClip) {
            auto *transformFx = ensureEffect(
                shieldClip, QStringLiteral("affine"),
                tr("Transform"), tr("Position and scale"));
            ensureStringParam(transformFx, QStringLiteral("transition.rect"),
                              tr("Geometry"),
                              QStringLiteral("0=25%/25%:50%x50%:100;100=0/0:100%x100%:100"));
            ensureEffect(shieldClip, QStringLiteral("fadeInBrightness"),
                         tr("Fade In (Brightness)"),
                         tr("Fade in from black"));
            if (includeLooney) {
                ensureEffect(shieldClip, QStringLiteral("fadeOutBrightness"),
                             tr("Fade Out (Brightness)"),
                             tr("Fade out to black"));
            }
        }

        if (captionTrack) {
            const QString captionPath = makeTextImage(captionText);
            int64_t captionDuration = includeLooney
                ? qMax<int64_t>(1, (replaceStart + fadeFrames) - captionStart)
                : qMax<int64_t>(1, totalFrames - captionStart);
            auto *captionClip = addClip(captionTrack, tr("Caption"),
                                        captionPath, captionStart,
                                        captionDuration);
            if (captionClip) {
                auto *transformFx = ensureEffect(
                    captionClip, QStringLiteral("affine"),
                    tr("Transform"), tr("Position and scale"));
                ensureStringParam(transformFx, QStringLiteral("transition.rect"),
                                  tr("Geometry"),
                                  QStringLiteral("0/-35:100%x100%:100"));
                if (includeLooney) {
                    ensureEffect(captionClip, QStringLiteral("fadeOutBrightness"),
                                 tr("Fade Out (Brightness)"),
                                 tr("Fade out to black"));
                }
            }
        }

        if (includeLooney && looneyTrack) {
            const QString looneyPath = makeTextImage(looneyText);
            const int64_t looneyDuration = qMax<int64_t>(1, totalFrames - replaceStart);
            auto *looneyClip = addClip(looneyTrack, tr("Looney Text"),
                                       looneyPath, replaceStart,
                                       looneyDuration);
            if (looneyClip) {
                ensureEffect(looneyClip, QStringLiteral("fadeInBrightness"),
                             tr("Fade In (Brightness)"),
                             tr("Fade in from black"));
            }
        }

        m_undoStack->endMacro();

        m_modified = true;
        deferRebuildTractor();
        m_timeline->refresh();
        m_announcer->announce(
            includeLooney
                ? tr("Intro stack created with rings, shield, caption, and Looney text.")
                : tr("Intro stack created with rings, shield, and caption."),
            Announcer::Priority::High);
    });

    m_actAddTextClip = new QAction(tr("Add Te&xt Overlay Clip…"), this);
    connect(m_actAddTextClip, &QAction::triggered, this, [this]() {
        bool ok = false;
        const QString text = QInputDialog::getMultiLineText(
            this,
            tr("Add Text Overlay Clip"),
            tr("Text content:"),
            QString(),
            &ok);
        if (!ok || text.trimmed().isEmpty())
            return;

        bool durOk = false;
        const double seconds = QInputDialog::getDouble(
            this,
            tr("Text Duration"),
            tr("Duration in seconds:"),
            2.0,
            0.2,
            120.0,
            1,
            &durOk);
        if (!durOk)
            return;

        auto *tl = m_project->timeline();
        if (!tl) {
            m_announcer->announce(tr("Timeline is not available."),
                                  Announcer::Priority::High);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }

        if (tl->trackCount() == 0) {
            m_undoStack->push(
                new AddTrackCommand(
                    tl,
                    new Track(tr("Video 1"), Track::Type::Video)));
        }

        Track *targetTrack = nullptr;
        auto *current = tl->trackAt(tl->currentTrackIndex());
        if (current && !current->isLocked() && current->type() == Track::Type::Video) {
            targetTrack = current;
        } else {
            for (auto *candidate : tl->tracks()) {
                if (candidate && !candidate->isLocked()
                    && candidate->type() == Track::Type::Video) {
                    targetTrack = candidate;
                    break;
                }
            }
        }

        if (!targetTrack) {
            int videoCount = 0;
            for (auto *t : tl->tracks()) {
                if (t && t->type() == Track::Type::Video)
                    ++videoCount;
            }
            auto *newTrack = new Track(
                tr("Video %1").arg(videoCount + 1), Track::Type::Video);
            m_undoStack->push(new AddTrackCommand(tl, newTrack));
            targetTrack = newTrack;
        }

        if (!targetTrack || targetTrack->isLocked()) {
            m_announcer->announce(
                tr("No unlocked video track available for text overlay."),
                Announcer::Priority::High);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }

        const int width = qMax(320, m_project->width());
        const int height = qMax(180, m_project->height());
        QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);

        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        QFont font;
        font.setPointSize(qMax(24, height / 12));
        font.setBold(true);
        painter.setFont(font);

        QPainterPath path;
        path.addText(0.0, 0.0, font, text);
        const QRectF b = path.boundingRect();
        QTransform transform;
        transform.translate((width - b.width()) / 2.0 - b.left(),
                            (height - b.height()) / 2.0 - b.top());
        QPainterPath centered = transform.map(path);

        painter.setPen(QPen(QColor(0, 0, 0, 220), 4.0,
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(QColor(255, 255, 255, 255));
        painter.drawPath(centered);
        painter.end();

        const QString baseDir = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation) + QStringLiteral("/text-overlays");
        QDir().mkpath(baseDir);
        const QString filePath = baseDir
            + QStringLiteral("/text_overlay_")
            + QUuid::createUuid().toString(QUuid::WithoutBraces)
            + QStringLiteral(".png");

        if (!image.save(filePath, "PNG")) {
            m_announcer->announce(tr("Failed to create text overlay image."),
                                  Announcer::Priority::High);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }

        const double fps = m_project->fps();
        const int64_t durationFrames = qMax<int64_t>(1,
            static_cast<int64_t>(seconds * fps));
        auto *clip = new Clip(
            tr("Text: %1").arg(text.left(24).trimmed()),
            filePath,
            TimeCode(0, fps),
            TimeCode(durationFrames - 1, fps));

        const int64_t desiredStart = tl->playheadPosition().frame();
        int64_t start = qMax<int64_t>(0, desiredStart);
        bool moved = true;
        while (moved) {
            moved = false;
            const int64_t end = start + durationFrames;
            for (auto *existing : targetTrack->clips()) {
                if (!existing)
                    continue;
                const int64_t exStart = existing->timelinePosition().frame();
                const int64_t exSpan = qMax<int64_t>(1, existing->duration().frame());
                const int64_t exEnd = exStart + exSpan;
                const bool overlaps = !(end <= exStart || start >= exEnd);
                if (overlaps) {
                    start = exEnd;
                    moved = true;
                    break;
                }
            }
        }
        clip->setTimelinePosition(TimeCode(start, fps));

        m_undoStack->push(new AddClipCommand(targetTrack, clip));
        m_modified = true;
        deferRebuildTractor();

        for (int i = 0; i < tl->trackCount(); ++i) {
            if (tl->trackAt(i) == targetTrack) {
                tl->setCurrentTrackIndex(i);
                tl->setCurrentClipIndex(targetTrack->clipCount() - 1);
                break;
            }
        }

        m_cues->play(AudioCueManager::Cue::ClipAdded);
        m_announcer->announce(
            tr("Added text overlay clip to %1 at %2.")
                .arg(targetTrack->name(),
                     clip->timelinePosition().toSpokenString()),
            Announcer::Priority::High);
    });

    m_actApplyAvatarPreset = new QAction(tr("Apply &Motion Preset…"), this);
    connect(m_actApplyAvatarPreset, &QAction::triggered, this, [this]() {
        auto *tl = m_project->timeline();
        auto *trk = tl ? tl->trackAt(tl->currentTrackIndex()) : nullptr;
        const int idx = tl ? tl->currentClipIndex() : -1;
        if (!trk || idx < 0 || idx >= trk->clipCount()) {
            m_announcer->announce(tr("Select a clip first."),
                                  Announcer::Priority::Normal);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }
        if (trk->isLocked()) {
            m_announcer->announce(tr("Track is locked."),
                                  Announcer::Priority::Normal);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }

        auto *clip = trk->clipAt(idx);
        QStringList presets;
        presets << tr("Center Overlay")
                << tr("Top Caption")
                << tr("Zoom In")
                << tr("Fade In")
                << tr("Fade Out");
        bool ok = false;
        const QString preset = QInputDialog::getItem(
            this, tr("Motion Preset"), tr("Preset:"),
            presets, 0, false, &ok);
        if (!ok)
            return;

        auto findEffect = [clip](const QString &sid) -> Effect * {
            for (auto *fx : clip->effects()) {
                if (fx && fx->serviceId() == sid)
                    return fx;
            }
            return nullptr;
        };

        auto ensureEffect = [this, clip, &findEffect](const QString &sid,
                                                      const QString &name,
                                                      const QString &desc) -> Effect * {
            if (auto *existing = findEffect(sid))
                return existing;
            auto *fx = new Effect(sid, name, desc);
            m_undoStack->push(new AddEffectCommand(clip, fx));
            return fx;
        };

        auto ensureStringParam = [](Effect *fx, const QString &id,
                                    const QString &display,
                                    const QString &value) {
            if (!fx)
                return;
            bool found = false;
            for (const auto &p : fx->parameters()) {
                if (p.id == id) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                EffectParameter param;
                param.id = id;
                param.displayName = display;
                param.type = QStringLiteral("string");
                param.defaultValue = value;
                param.currentValue = value;
                fx->addParameter(param);
            }
            fx->setParameterValue(id, value);
        };

        if (preset == presets.at(0)) {
            auto *fx = ensureEffect(QStringLiteral("affine"),
                                    tr("Transform"),
                                    tr("Position and scale"));
            ensureStringParam(fx, QStringLiteral("transition.rect"),
                              tr("Geometry"),
                              QStringLiteral("0/0:100%x100%:100"));
        } else if (preset == presets.at(1)) {
            auto *fx = ensureEffect(QStringLiteral("affine"),
                                    tr("Transform"),
                                    tr("Position and scale"));
            ensureStringParam(fx, QStringLiteral("transition.rect"),
                              tr("Geometry"),
                              QStringLiteral("0/-35:100%x100%:100"));
        } else if (preset == presets.at(2)) {
            auto *fx = ensureEffect(QStringLiteral("affine"),
                                    tr("Transform"),
                                    tr("Position and scale"));
            ensureStringParam(fx, QStringLiteral("transition.rect"),
                              tr("Geometry"),
                              QStringLiteral("0=25%/25%:50%x50%:100;100=0/0:100%x100%:100"));
            ensureEffect(QStringLiteral("fadeInBrightness"),
                         tr("Fade In (Brightness)"),
                         tr("Fade in from black"));
        } else if (preset == presets.at(3)) {
            ensureEffect(QStringLiteral("fadeInBrightness"),
                         tr("Fade In (Brightness)"),
                         tr("Fade in from black"));
        } else if (preset == presets.at(4)) {
            ensureEffect(QStringLiteral("fadeOutBrightness"),
                         tr("Fade Out (Brightness)"),
                         tr("Fade out to black"));
        }

        m_modified = true;
        rebuildTractor();
        m_announcer->announce(
            tr("Applied preset %1 to %2.").arg(preset, clip->name()),
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
            m_cues->play(AudioCueManager::Cue::Error);
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

    // ── Remove Marker at Playhead ────────────────────────────────
    m_actRemoveMarker = new QAction(tr("Re&move Marker at Playhead"), this);
    m_actRemoveMarker->setShortcut(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M));
    connect(m_actRemoveMarker, &QAction::triggered, this, [this]() {
        auto *tl = m_project->timeline();
        const TimeCode pos = tl->playheadPosition();
        int found = -1;
        for (int i = 0; i < tl->markers().size(); ++i) {
            if (tl->markers().at(i)->position() == pos) {
                found = i;
                break;
            }
        }
        if (found < 0) {
            m_announcer->announce(tr("No marker at playhead."),
                                  Announcer::Priority::Normal);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }
        const QString name = tl->markers().at(found)->name();
        m_undoStack->push(new RemoveMarkerCommand(tl, found));
        m_announcer->announce(
            tr("Removed marker \"%1\".").arg(name),
            Announcer::Priority::High);
    });

    // ── Move clip between tracks (Shift+Up / Shift+Down) ─────────
    m_actMoveClipUp = new QAction(tr("Move Clip to Track A&bove"), this);
    connect(m_actMoveClipUp, &QAction::triggered, this, [this]() {
        auto *tl  = m_project->timeline();
        int trkIdx = tl->currentTrackIndex();
        auto *trk = tl->trackAt(trkIdx);
        int clipIdx = tl->currentClipIndex();
        if (!trk || clipIdx < 0 || clipIdx >= trk->clipCount()) {
            m_announcer->announce(tr("No clip selected."),
                                  Announcer::Priority::Normal);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }
        if (trk->isLocked()) {
            m_announcer->announce(tr("Track is locked."),
                                  Announcer::Priority::Normal);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }
        if (trkIdx <= 0) {
            m_announcer->announce(tr("Already on the first track."),
                                  Announcer::Priority::Normal);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }
        auto *dst = tl->trackAt(trkIdx - 1);
        if (dst->isLocked()) {
            m_announcer->announce(tr("Destination track is locked."),
                                  Announcer::Priority::Normal);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }
        m_undoStack->push(new MoveClipBetweenTracksCommand(
            trk, clipIdx, dst));
        tl->setCurrentTrackIndex(trkIdx - 1);
        m_modified = true;
        rebuildTractor();
        m_announcer->announce(
            tr("Clip moved to %1.").arg(dst->name()),
            Announcer::Priority::High);
    });

    m_actMoveClipDown = new QAction(tr("Move Clip to Track Be&low"), this);
    connect(m_actMoveClipDown, &QAction::triggered, this, [this]() {
        auto *tl  = m_project->timeline();
        int trkIdx = tl->currentTrackIndex();
        auto *trk = tl->trackAt(trkIdx);
        int clipIdx = tl->currentClipIndex();
        if (!trk || clipIdx < 0 || clipIdx >= trk->clipCount()) {
            m_announcer->announce(tr("No clip selected."),
                                  Announcer::Priority::Normal);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }
        if (trk->isLocked()) {
            m_announcer->announce(tr("Track is locked."),
                                  Announcer::Priority::Normal);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }
        if (trkIdx >= tl->trackCount() - 1) {
            m_announcer->announce(tr("Already on the last track."),
                                  Announcer::Priority::Normal);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }
        auto *dst = tl->trackAt(trkIdx + 1);
        if (dst->isLocked()) {
            m_announcer->announce(tr("Destination track is locked."),
                                  Announcer::Priority::Normal);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }
        m_undoStack->push(new MoveClipBetweenTracksCommand(
            trk, clipIdx, dst));
        tl->setCurrentTrackIndex(trkIdx + 1);
        m_modified = true;
        rebuildTractor();
        m_announcer->announce(
            tr("Clip moved to %1.").arg(dst->name()),
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

    // ── Transport shortcuts (global) ─────────────────────────────────
    m_actPlayPause = new QAction(tr("Play / Pause"), this);
    m_actPlayPause->setShortcut(Qt::Key_Space);
    connect(m_actPlayPause, &QAction::triggered, this, [this]() {
        m_playback->togglePlayPause();
        const bool playing =
            (m_playback->state() == PlaybackController::State::Playing);
        m_announcer->announce(playing ? tr("Playing") : tr("Paused"),
                              Announcer::Priority::High);
    });
    addAction(m_actPlayPause);

    m_actJRewind = new QAction(tr("Rewind (J)"), this);
    m_actJRewind->setShortcut(Qt::Key_J);
    connect(m_actJRewind, &QAction::triggered, this, [this]() {
        m_playback->playReverse();
        const int absSpeed = static_cast<int>(qAbs(m_playback->speed()));
        m_announcer->announce(
            absSpeed > 1 ? tr("Rewinding %1x").arg(absSpeed)
                         : tr("Rewinding"),
            Announcer::Priority::High);
    });
    addAction(m_actJRewind);

    m_actLForward = new QAction(tr("Fast Forward (L)"), this);
    m_actLForward->setShortcut(Qt::Key_L);
    connect(m_actLForward, &QAction::triggered, this, [this]() {
        m_playback->playForward();
        const int absSpeed = static_cast<int>(qAbs(m_playback->speed()));
        m_announcer->announce(
            absSpeed > 1 ? tr("Fast forwarding %1x").arg(absSpeed)
                         : tr("Fast forwarding"),
            Announcer::Priority::High);
    });
    addAction(m_actLForward);

    m_actFrameBack = new QAction(tr("Step Frame Back"), this);
    m_actFrameBack->setShortcut(Qt::Key_Comma);
    connect(m_actFrameBack, &QAction::triggered, this, [this]() {
        m_playback->stepFrames(-1);
        m_announcer->announce(
            tr("Frame %1").arg(m_playback->position()),
            Announcer::Priority::Low);
    });
    addAction(m_actFrameBack);

    m_actFrameForward = new QAction(tr("Step Frame Forward"), this);
    m_actFrameForward->setShortcut(Qt::Key_Period);
    connect(m_actFrameForward, &QAction::triggered, this, [this]() {
        m_playback->stepFrames(1);
        m_announcer->announce(
            tr("Frame %1").arg(m_playback->position()),
            Announcer::Priority::Low);
    });
    addAction(m_actFrameForward);

    // K key = stop transport (standard JKL model)
    auto *actKStop = new QAction(tr("Stop Transport (K)"), this);
    actKStop->setShortcut(Qt::Key_K);
    connect(actKStop, &QAction::triggered,
            m_playback, &PlaybackController::stopTransport);
    addAction(actKStop);

    // Go to timecode (Ctrl+G)
    m_actGoToTimecode = new QAction(tr("&Go to Timecode…"), this);
    m_actGoToTimecode->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(m_actGoToTimecode, &QAction::triggered, this, [this]() {
        bool ok = false;
        const QString input = QInputDialog::getText(
            this, tr("Go to Timecode"),
            tr("Enter timecode (HH:MM:SS:FF):"),
            QLineEdit::Normal, QString(), &ok);
        if (!ok || input.isEmpty()) return;

        const TimeCode tc = TimeCode::fromString(input, m_project->fps());
        if (tc.frame() < 0) {
            m_announcer->announce(tr("Invalid timecode."),
                                  Announcer::Priority::Normal);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }
        m_project->timeline()->setPlayheadPosition(tc);
        m_playback->seek(static_cast<int>(tc.frame()));
        m_announcer->announce(
            tr("Moved to %1.").arg(tc.toSpokenString()),
            Announcer::Priority::High);
    });
    addAction(m_actGoToTimecode);

    // ── Mute / Lock track toggles ─────────────────────────────────
    m_actMuteTrack = new QAction(tr("Toggle Track &Mute"), this);
    connect(m_actMuteTrack, &QAction::triggered, this, [this]() {
        auto *tl  = m_project->timeline();
        auto *trk = tl->trackAt(tl->currentTrackIndex());
        if (!trk) return;
        m_undoStack->push(new ToggleMuteTrackCommand(trk));
        m_modified = true;
        rebuildTractor();
        m_announcer->announce(
            trk->isMuted() ? tr("%1 muted.").arg(trk->name())
                           : tr("%1 unmuted.").arg(trk->name()),
            Announcer::Priority::High);
    });

    m_actLockTrack = new QAction(tr("Toggle Track L&ock"), this);
    connect(m_actLockTrack, &QAction::triggered, this, [this]() {
        auto *tl  = m_project->timeline();
        auto *trk = tl->trackAt(tl->currentTrackIndex());
        if (!trk) return;
        m_undoStack->push(new ToggleLockTrackCommand(trk));
        m_modified = true;
        m_announcer->announce(
            trk->isLocked() ? tr("%1 locked.").arg(trk->name())
                            : tr("%1 unlocked.").arg(trk->name()),
            Announcer::Priority::High);
    });

    // ── Quick-access: focus Media / Effects panels ────────────────
    m_actFocusMedia = new QAction(tr("Focus &Media Browser"), this);
    connect(m_actFocusMedia, &QAction::triggered, this, [this]() {
        m_media->setFocus();
        m_announcer->announce(tr("Media browser"),
                              Announcer::Priority::Normal);
    });

    m_actFocusEffects = new QAction(tr("Focus E&ffects Browser"), this);
    connect(m_actFocusEffects, &QAction::triggered, this, [this]() {
        m_effects->setFocus();
        m_announcer->announce(tr("Effects browser"),
                              Announcer::Priority::Normal);
    });

    m_actFocusProperties = new QAction(tr("Focus &Properties Panel"), this);
    connect(m_actFocusProperties, &QAction::triggered, this, [this]() {
        m_properties->setFocus();
        m_announcer->announce(tr("Properties panel"),
                              Announcer::Priority::Normal);
    });

    m_actFocusTimeline = new QAction(tr("Focus &Timeline"), this);
    connect(m_actFocusTimeline, &QAction::triggered, this, [this]() {
        m_timeline->setFocus();
        m_announcer->announce(tr("Timeline"),
                              Announcer::Priority::Normal);
    });

    m_actFocusTransport = new QAction(tr("Focus T&ransport"), this);
    connect(m_actFocusTransport, &QAction::triggered, this, [this]() {
        m_transport->setFocus();
        m_announcer->announce(tr("Transport"),
                              Announcer::Priority::Normal);
    });

    m_actAnnounceContext = new QAction(tr("Announce Current Conte&xt"), this);
    connect(m_actAnnounceContext, &QAction::triggered, this, [this]() {
        auto *tl = m_project->timeline();
        auto *trk = tl ? tl->trackAt(tl->currentTrackIndex()) : nullptr;

        QString focusName;
        if (auto *fw = QApplication::focusWidget()) {
            focusName = fw->accessibleName();
            if (focusName.isEmpty())
                focusName = fw->windowTitle();
            if (focusName.isEmpty())
                focusName = fw->objectName();
        }
        if (focusName.isEmpty())
            focusName = tr("Unknown focus");

        QString message;
        switch (m_contextVerbosity) {
        case ContextVerbosity::Short:
            message = tr("Focus: %1.").arg(focusName);
            if (tl) {
                message += QLatin1Char(' ')
                           + tr("Playhead %1.")
                                 .arg(tl->playheadPosition().toSpokenString());
            }
            break;

        case ContextVerbosity::Normal:
            message = tr("Focus: %1.").arg(focusName);
            if (tl) {
                message += QLatin1Char(' ')
                           + tr("Playhead %1.")
                                 .arg(tl->playheadPosition().toSpokenString());
            }
            if (trk) {
                message += QLatin1Char(' ')
                           + tr("Track %1 of %2: %3.")
                                 .arg(tl->currentTrackIndex() + 1)
                                 .arg(tl->trackCount())
                                 .arg(trk->name());

                const int clipIdx = tl->currentClipIndex();
                if (clipIdx >= 0 && clipIdx < trk->clipCount()) {
                    message += QLatin1Char(' ')
                               + tr("Clip %1 of %2: %3.")
                                     .arg(clipIdx + 1)
                                     .arg(trk->clipCount())
                                     .arg(trk->clipAt(clipIdx)->name());
                }
            }
            break;

        case ContextVerbosity::Detailed:
            message = tr("Focus: %1.").arg(focusName);
            if (tl) {
                message += QLatin1Char(' ')
                           + tr("Playhead %1.")
                                 .arg(tl->playheadPosition().toSpokenString());
                message += QLatin1Char(' ')
                           + tr("%1 marker(s) in project.")
                                 .arg(tl->markers().size());
            }
            if (trk) {
                message += QLatin1Char(' ')
                           + tr("Track %1 of %2: %3.")
                                 .arg(tl->currentTrackIndex() + 1)
                                 .arg(tl->trackCount())
                                 .arg(trk->name());
                message += QLatin1Char(' ')
                           + tr("Track type %1. %2. %3.")
                                 .arg(trk->type() == Track::Type::Audio ? tr("Audio") : tr("Video"))
                                 .arg(trk->isMuted() ? tr("Muted") : tr("Unmuted"))
                                 .arg(trk->isLocked() ? tr("Locked") : tr("Unlocked"));

                const int clipIdx = tl->currentClipIndex();
                if (clipIdx >= 0 && clipIdx < trk->clipCount()) {
                    auto *clip = trk->clipAt(clipIdx);
                    message += QLatin1Char(' ')
                               + tr("Clip %1 of %2: %3.")
                                     .arg(clipIdx + 1)
                                     .arg(trk->clipCount())
                                     .arg(clip->name());
                    message += QLatin1Char(' ')
                               + tr("Clip starts at %1 and lasts %2.")
                                     .arg(clip->timelinePosition().toSpokenString())
                                     .arg(clip->duration().toSpokenString());
                    message += QLatin1Char(' ')
                               + tr("%1 effect(s) on clip.")
                                     .arg(clip->effects().size());
                }
            }
            break;
        }

        m_announcer->announce(message, Announcer::Priority::High);
    });

    m_actCycleContextVerbosity = new QAction(
        tr("Cycle Conte&xt Verbosity"), this);
    connect(m_actCycleContextVerbosity, &QAction::triggered,
            this, [this]() {
                switch (m_contextVerbosity) {
                case ContextVerbosity::Short:
                    m_contextVerbosity = ContextVerbosity::Normal;
                    m_announcer->announce(tr("Context verbosity: normal."),
                                          Announcer::Priority::Normal);
                    break;
                case ContextVerbosity::Normal:
                    m_contextVerbosity = ContextVerbosity::Detailed;
                    m_announcer->announce(tr("Context verbosity: detailed."),
                                          Announcer::Priority::Normal);
                    break;
                case ContextVerbosity::Detailed:
                    m_contextVerbosity = ContextVerbosity::Short;
                    m_announcer->announce(tr("Context verbosity: short."),
                                          Announcer::Priority::Normal);
                    break;
                }
            });

    m_actToggleMarkerJumpSnap = new QAction(
        tr("Toggle Marker Jump &Snap"), this);
    m_actToggleMarkerJumpSnap->setCheckable(true);
    m_actToggleMarkerJumpSnap->setChecked(true);
    connect(m_actToggleMarkerJumpSnap, &QAction::toggled,
            this, [this](bool enabled) {
                if (m_timeline)
                    m_timeline->setMarkerJumpSnapEnabled(enabled);
                m_announcer->announce(
                    enabled ? tr("Marker jump snap on.")
                            : tr("Marker jump snap off."),
                    Announcer::Priority::Normal);
            });

    m_actAnnounceShortcuts = new QAction(tr("Announce &Keyboard Help"), this);
    connect(m_actAnnounceShortcuts, &QAction::triggered, this, [this]() {
        const QString help = tr(
            "Keyboard help. "
            "Space play or pause, J rewind, K stop, L fast forward. "
            "Timeline arrows move tracks and clips. "
            "Page Up and Page Down jump five clips. "
            "Control plus Page Up and Control plus Page Down jump to previous or next non-empty track. "
            "Control plus Home and Control plus End jump to first or last clip on the current track. "
            "Control 1 focuses timeline, Control 2 focuses transport, Control I media, Control E effects, Control P properties.");
        m_announcer->announce(help, Announcer::Priority::High);
    });

    // ── Move track up / down ──────────────────────────────────────────
    m_actMoveTrackUp = new QAction(tr("Move Track &Up"), this);
    connect(m_actMoveTrackUp, &QAction::triggered, this, [this]() {
        auto *tl = m_project->timeline();
        int idx = tl->currentTrackIndex();
        if (idx <= 0) {
            m_announcer->announce(tr("Already the first track."),
                                  Announcer::Priority::Normal);
            return;
        }
        m_undoStack->push(new MoveTrackCommand(tl, idx, idx - 1));
        tl->setCurrentTrackIndex(idx - 1);
        m_modified = true;
        m_timeline->refresh();
        m_announcer->announce(
            tr("%1 moved up.").arg(tl->trackAt(idx - 1)->name()),
            Announcer::Priority::High);
    });

    m_actMoveTrackDown = new QAction(tr("Move Track &Down"), this);
    connect(m_actMoveTrackDown, &QAction::triggered, this, [this]() {
        auto *tl = m_project->timeline();
        int idx = tl->currentTrackIndex();
        if (idx < 0 || idx >= tl->trackCount() - 1) {
            m_announcer->announce(tr("Already the last track."),
                                  Announcer::Priority::Normal);
            return;
        }
        m_undoStack->push(new MoveTrackCommand(tl, idx, idx + 1));
        tl->setCurrentTrackIndex(idx + 1);
        m_modified = true;
        m_timeline->refresh();
        m_announcer->announce(
            tr("%1 moved down.").arg(tl->trackAt(idx + 1)->name()),
            Announcer::Priority::High);
    });

    // ── Solo track ─────────────────────────────────────────────────
    m_actSoloTrack = new QAction(tr("&Solo Current Track"), this);
    connect(m_actSoloTrack, &QAction::triggered, this, [this]() {
        auto *tl  = m_project->timeline();
        auto *cur = tl->trackAt(tl->currentTrackIndex());
        if (!cur) return;

        // Check if this track is already soloed (all others muted, this unmuted)
        bool alreadySoloed = !cur->isMuted();
        for (int i = 0; alreadySoloed && i < tl->trackCount(); ++i) {
            auto *t = tl->trackAt(i);
            if (t != cur && !t->isMuted())
                alreadySoloed = false;
        }

        m_undoStack->push(
            new SoloTrackCommand(tl, tl->currentTrackIndex()));
        m_modified = true;
        rebuildTractor();

        if (alreadySoloed) {
            m_announcer->announce(
                tr("All tracks unmuted."), Announcer::Priority::High);
        } else {
            m_announcer->announce(
                tr("%1 soloed.").arg(cur->name()),
                Announcer::Priority::High);
        }
    });

    // ── Nudge clip position (frame-accurate repositioning) ────────
    m_actNudgeClipLeft = new QAction(tr("Nudge Clip &Left (−1 frame)"), this);
    connect(m_actNudgeClipLeft, &QAction::triggered, this, [this]() {
        auto *tl  = m_project->timeline();
        auto *trk = tl->trackAt(tl->currentTrackIndex());
        int   idx = tl->currentClipIndex();
        if (!trk || idx < 0 || idx >= trk->clipCount()) {
            m_announcer->announce(tr("No clip selected."),
                                  Announcer::Priority::Normal);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }
        if (trk->isLocked()) {
            m_announcer->announce(tr("Track is locked."),
                                  Announcer::Priority::Normal);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }
        auto *clip = trk->clipAt(idx);
        const int64_t frame = clip->timelinePosition().frame();
        if (frame <= 0) {
            m_announcer->announce(tr("Clip is already at the start."),
                                  Announcer::Priority::Normal);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }
        m_undoStack->push(new NudgeClipPositionCommand(
            clip, TimeCode(frame - 1, m_project->fps())));
        m_modified = true;
        rebuildTractor();
        m_announcer->announce(
            tr("Clip at %1.").arg(clip->timelinePosition().toSpokenString()),
            Announcer::Priority::Normal);
    });

    m_actNudgeClipRight = new QAction(tr("Nudge Clip &Right (+1 frame)"), this);
    connect(m_actNudgeClipRight, &QAction::triggered, this, [this]() {
        auto *tl  = m_project->timeline();
        auto *trk = tl->trackAt(tl->currentTrackIndex());
        int   idx = tl->currentClipIndex();
        if (!trk || idx < 0 || idx >= trk->clipCount()) {
            m_announcer->announce(tr("No clip selected."),
                                  Announcer::Priority::Normal);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }
        if (trk->isLocked()) {
            m_announcer->announce(tr("Track is locked."),
                                  Announcer::Priority::Normal);
            m_cues->play(AudioCueManager::Cue::Error);
            return;
        }
        auto *clip = trk->clipAt(idx);
        m_undoStack->push(new NudgeClipPositionCommand(
            clip, TimeCode(clip->timelinePosition().frame() + 1,
                           m_project->fps())));
        m_modified = true;
        rebuildTractor();
        m_announcer->announce(
            tr("Clip at %1.").arg(clip->timelinePosition().toSpokenString()),
            Announcer::Priority::Normal);
    });
}

void MainWindow::createMenus()
{
    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_actNew);
    fileMenu->addAction(m_actOpen);
    m_recentMenu = fileMenu->addMenu(tr("Recent &Projects"));
    m_recentMenu->setAccessibleName(tr("Recent projects"));
    updateRecentMenu();
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
    timelineMenu->addAction(m_actBuildIntroStack);
    timelineMenu->addAction(m_actAddTextClip);
    timelineMenu->addAction(m_actApplyAvatarPreset);
    timelineMenu->addSeparator();
    timelineMenu->addAction(m_actAddMarker);
    timelineMenu->addAction(m_actRemoveMarker);
    timelineMenu->addAction(m_actToggleMarkerJumpSnap);
    timelineMenu->addSeparator();
    timelineMenu->addAction(m_actAddTransition);
    timelineMenu->addSeparator();
    timelineMenu->addAction(m_actMuteTrack);
    timelineMenu->addAction(m_actLockTrack);
    timelineMenu->addAction(m_actSoloTrack);
    timelineMenu->addSeparator();
    timelineMenu->addAction(m_actMoveTrackUp);
    timelineMenu->addAction(m_actMoveTrackDown);
    timelineMenu->addSeparator();
    timelineMenu->addAction(m_actMoveClipUp);
    timelineMenu->addAction(m_actMoveClipDown);
    timelineMenu->addSeparator();
    timelineMenu->addAction(m_actNudgeClipLeft);
    timelineMenu->addAction(m_actNudgeClipRight);

    auto *transportMenu = menuBar()->addMenu(tr("T&ransport"));
    transportMenu->addAction(m_actPlayPause);
    transportMenu->addAction(m_actJRewind);
    transportMenu->addAction(m_actLForward);
    transportMenu->addSeparator();
    transportMenu->addAction(m_actFrameBack);
    transportMenu->addAction(m_actFrameForward);
    transportMenu->addSeparator();
    transportMenu->addAction(m_actGoToTimecode);

    auto *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(m_actFocusTimeline);
    viewMenu->addAction(m_actFocusTransport);
    viewMenu->addSeparator();
    viewMenu->addAction(m_actFocusMedia);
    viewMenu->addAction(m_actFocusEffects);
    viewMenu->addAction(m_actFocusProperties);
    viewMenu->addSeparator();
    viewMenu->addAction(m_actAnnounceContext);
    viewMenu->addAction(m_actCycleContextVerbosity);

    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(m_actAnnounceShortcuts);
    helpMenu->addSeparator();
    helpMenu->addAction(m_actWizard);
    helpMenu->addAction(m_actAbout);
}

void MainWindow::createDockWidgets()
{
    // Central widget: a splitter holding the preview and timeline
    auto *centralSplitter = new QSplitter(Qt::Vertical, this);

    // Video preview (top of centre)
    m_preview = new VideoPreviewWidget(this);
    centralSplitter->addWidget(m_preview);

    // Timeline (bottom of centre)
    m_timeline = new TimelineWidget(
        m_project->timeline(), m_announcer, m_cues, m_engine, this);
    m_timeline->setMarkerJumpSnapEnabled(
        m_actToggleMarkerJumpSnap ? m_actToggleMarkerJumpSnap->isChecked()
                                  : true);
    centralSplitter->addWidget(m_timeline);

    // Give the preview roughly 2/3 of the space, timeline 1/3
    centralSplitter->setStretchFactor(0, 2);
    centralSplitter->setStretchFactor(1, 1);
    setCentralWidget(centralSplitter);

    // Route video frames from the playback consumer to the preview widget
    connect(m_playback, &PlaybackController::frameRendered,
            m_preview, &VideoPreviewWidget::updateFrame);

    // Transport bar (bottom)
    m_transport = new TransportBar(
        m_playback, m_project->timeline(), m_announcer, this);
    auto *transportDock = new QDockWidget(tr("Transport"), this);
    transportDock->setObjectName(QStringLiteral("TransportDock"));
    transportDock->setWidget(m_transport);
    transportDock->setFeatures(QDockWidget::DockWidgetMovable);
    transportDock->setAccessibleDescription(
        tr("Transport controls. Space to play/pause, J to rewind, L to fast forward."));
    addDockWidget(Qt::BottomDockWidgetArea, transportDock);

    // Media browser (left)
    m_media = new MediaBrowser(m_announcer, m_engine, this);
    auto *mediaDock = new QDockWidget(tr("Media"), this);
    mediaDock->setObjectName(QStringLiteral("MediaDock"));
    mediaDock->setWidget(m_media);
    mediaDock->setAccessibleDescription(
        tr("Media browser. Browse and import media files into your project."));
    addDockWidget(Qt::LeftDockWidgetArea, mediaDock);

    // Properties panel (right)
    m_properties = new PropertiesPanel(m_announcer, m_undoStack, this);
    auto *propDock = new QDockWidget(tr("Properties"), this);
    propDock->setObjectName(QStringLiteral("PropertiesDock"));
    propDock->setWidget(m_properties);
    propDock->setAccessibleDescription(
        tr("Properties panel. Ctrl+P to focus. Shows details for the selected clip or track."));
    addDockWidget(Qt::RightDockWidgetArea, propDock);

    // Rebuild tractor when a clip is trimmed from the properties panel
    connect(m_properties, &PropertiesPanel::clipTrimmed,
            this, &MainWindow::rebuildTractor);

    // Rebuild tractor when effects are changed from the properties panel
    connect(m_properties, &PropertiesPanel::effectChanged,
            this, &MainWindow::rebuildTractor);

    // Effects browser (right, tabbed with properties)
    m_effects = new EffectsBrowser(m_catalog, m_announcer, this);
    auto *effectsDock = new QDockWidget(tr("Effects"), this);
    effectsDock->setObjectName(QStringLiteral("EffectsDock"));
    effectsDock->setWidget(m_effects);
    effectsDock->setAccessibleDescription(
        tr("Effects browser. Browse and apply audio and video effects."));
    tabifyDockWidget(propDock, effectsDock);

    // Keep effects browser out of tab order when its dock tab is hidden
    connect(effectsDock, &QDockWidget::visibilityChanged,
            m_effects, &EffectsBrowser::setTabOrderParticipation);
    // Start with effects hidden (properties tab is on top after tabify)
    m_effects->setTabOrderParticipation(false);

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
                if (filePath.isEmpty()) {
                    m_announcer->announce(
                        tr("No file path available."),
                        Announcer::Priority::High);
                    return;
                }

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

                // Probe the file with MLT to get its duration.
                // If probing fails we still add the clip with a fallback
                // duration so the timeline is never silently empty.
                const double fps = m_project->fps();
                int length = probeMediaLengthFrames(
                    filePath, static_cast<int>(fps * 5.0));

                qDebug() << "Media import: probed length =" << length
                         << "frames for" << filePath;

                TimeCode inPt(0, fps);
                TimeCode outPt(length > 0 ? length - 1 : 0, fps);

                // Place the clip at the playhead position
                auto *clip = new Clip(
                    QFileInfo(filePath).fileName(),
                    filePath, inPt, outPt);
                const int64_t desiredStartFrame =
                    tl->playheadPosition().frame();

                // Add to the currently selected track
                auto *trk = tl->trackAt(tl->currentTrackIndex());

                const QString ext = QFileInfo(filePath).suffix().toLower();
                const QStringList audioExt = {
                    QStringLiteral("wav"), QStringLiteral("mp3"),
                    QStringLiteral("flac"), QStringLiteral("ogg"),
                    QStringLiteral("m4a"), QStringLiteral("aac"),
                    QStringLiteral("wma"), QStringLiteral("aiff")
                };
                const Track::Type preferredType =
                    audioExt.contains(ext) ? Track::Type::Audio
                                           : Track::Type::Video;

                auto pickTargetTrack = [&](Track::Type preferred) -> Track * {
                    auto *current = tl->trackAt(tl->currentTrackIndex());
                    if (current && !current->isLocked()
                        && current->type() == preferred) {
                        return current;
                    }

                    for (auto *candidate : tl->tracks()) {
                        if (candidate && !candidate->isLocked()
                            && candidate->type() == preferred) {
                            return candidate;
                        }
                    }

                    return nullptr;
                };

                trk = pickTargetTrack(preferredType);
                if (!trk) {
                    int sameTypeCount = 0;
                    for (auto *existing : tl->tracks()) {
                        if (existing && existing->type() == preferredType)
                            ++sameTypeCount;
                    }

                    const QString baseName =
                        preferredType == Track::Type::Audio ? tr("Audio")
                                                            : tr("Video");
                    auto *newTrack = new Track(
                        tr("%1 %2").arg(baseName).arg(sameTypeCount + 1),
                        preferredType);
                    m_undoStack->push(new AddTrackCommand(tl, newTrack));
                    trk = newTrack;
                }

                if (!trk) {
                    m_announcer->announce(
                        tr("No track available. Add a track first."),
                        Announcer::Priority::High);
                    delete clip;
                    return;
                }

                const int64_t clipSpanFrames =
                    qMax<int64_t>(1, clip->duration().frame());
                auto findAvailableStart = [&](int64_t desiredStart) {
                    int64_t start = qMax<int64_t>(0, desiredStart);
                    bool moved = true;
                    while (moved) {
                        moved = false;
                        const int64_t end = start + clipSpanFrames;
                        for (auto *existing : trk->clips()) {
                            if (!existing)
                                continue;
                            const int64_t exStart =
                                existing->timelinePosition().frame();
                            const int64_t exSpan =
                                qMax<int64_t>(1, existing->duration().frame());
                            const int64_t exEnd = exStart + exSpan;

                            const bool overlaps = !(end <= exStart || start >= exEnd);
                            if (overlaps) {
                                start = exEnd;
                                moved = true;
                                break;
                            }
                        }
                    }
                    return start;
                };

                const int64_t placedStartFrame =
                    findAvailableStart(desiredStartFrame);
                clip->setTimelinePosition(TimeCode(placedStartFrame, fps));

                m_undoStack->push(new AddClipCommand(trk, clip));
                m_modified = true;
                deferRebuildTractor();

                for (int i = 0; i < tl->trackCount(); ++i) {
                    if (tl->trackAt(i) == trk) {
                        tl->setCurrentTrackIndex(i);
                        tl->setCurrentClipIndex(trk->clipCount() - 1);
                        break;
                    }
                }

                m_cues->play(AudioCueManager::Cue::ClipAdded);
                QString placementNote;
                if (placedStartFrame != desiredStartFrame) {
                    const TimeCode delta(
                        qAbs(placedStartFrame - desiredStartFrame), fps);
                    placementNote = tr("Placed at next free position, %1 from playhead.")
                                        .arg(delta.toSpokenString());
                }
                QString addedMessage = tr("Added %1 to %2 at %3 (%4).")
                    .arg(clip->name(), trk->name(),
                         clip->timelinePosition().toSpokenString(),
                         clip->duration().toSpokenString());
                if (!placementNote.isEmpty())
                    addedMessage += QLatin1Char(' ') + placementNote;
                m_announcer->announce(
                    addedMessage,
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
                    m_cues->play(AudioCueManager::Cue::Error);
                    return;
                }
                if (trk->isLocked()) {
                    m_announcer->announce(tr("Track is locked."),
                                          Announcer::Priority::Normal);
                    m_cues->play(AudioCueManager::Cue::Error);
                    return;
                }

                // Look up curated display name from the catalog
                const auto *entry = m_catalog->findByServiceId(serviceId);
                const QString displayName = entry ? entry->displayName
                                                  : serviceId;

                auto *clip   = trk->clipAt(idx);
                auto *effect = new Effect(serviceId, displayName, 
                    entry ? entry->description : QString());
                m_undoStack->push(new AddEffectCommand(clip, effect));
                m_modified = true;
                rebuildTractor();

                m_announcer->announce(
                    tr("Applied effect %1 to %2.")
                        .arg(displayName, clip->name()),
                    Announcer::Priority::High);
            });
}

int MainWindow::probeMediaLengthFrames(const QString &filePath,
                                       int fallbackFrames) const
{
    if (filePath.isEmpty())
        return fallbackFrames;

    int length = 0;
    auto *profile = m_engine ? m_engine->compositionProfile() : nullptr;
    if (profile) {
        const QByteArray pathUtf8 = filePath.toUtf8();
        Mlt::Producer probe(*profile, pathUtf8.constData());
        if (!probe.is_valid()) {
            const QByteArray avfPath = "avformat:" + pathUtf8;
            Mlt::Producer probe2(*profile, avfPath.constData());
            if (probe2.is_valid())
                length = probe2.get_length();
        } else {
            length = probe.get_length();
        }
    }

    return length > 0 ? length : fallbackFrames;
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
    sm.registerAction(QStringLiteral("timeline.buildIntroStack"), m_actBuildIntroStack,
                      QKeySequence(QStringLiteral("Ctrl+Shift+B")));
    sm.registerAction(QStringLiteral("timeline.addTextClip"), m_actAddTextClip,
                      QKeySequence(QStringLiteral("Ctrl+Shift+T")));
    sm.registerAction(QStringLiteral("timeline.applyAvatarPreset"), m_actApplyAvatarPreset,
                      QKeySequence(QStringLiteral("Ctrl+Shift+P")));
    sm.registerAction(QStringLiteral("timeline.addMarker"), m_actAddMarker,
                      QKeySequence(QStringLiteral("Shift+M")));
    sm.registerAction(QStringLiteral("timeline.addTransition"), m_actAddTransition,
                      QKeySequence(QStringLiteral("Shift+T")));

    // Transport shortcuts
    sm.registerAction(QStringLiteral("transport.playPause"), m_actPlayPause,
                      QKeySequence(Qt::Key_Space));
    sm.registerAction(QStringLiteral("transport.rewind"), m_actJRewind,
                      QKeySequence(Qt::Key_J));
    sm.registerAction(QStringLiteral("transport.forward"), m_actLForward,
                      QKeySequence(Qt::Key_L));
    sm.registerAction(QStringLiteral("transport.frameBack"), m_actFrameBack,
                      QKeySequence(Qt::Key_Comma));
    sm.registerAction(QStringLiteral("transport.frameForward"), m_actFrameForward,
                      QKeySequence(Qt::Key_Period));

    // Track mute / lock
    sm.registerAction(QStringLiteral("timeline.muteTrack"), m_actMuteTrack,
                      QKeySequence(QStringLiteral("Ctrl+M")));
    sm.registerAction(QStringLiteral("timeline.lockTrack"), m_actLockTrack,
                      QKeySequence(QStringLiteral("Ctrl+Shift+L")));

    // View / focus panel shortcuts
    sm.registerAction(QStringLiteral("view.focusTimeline"), m_actFocusTimeline,
                      QKeySequence(QStringLiteral("Ctrl+1")));
    sm.registerAction(QStringLiteral("view.focusTransport"), m_actFocusTransport,
                      QKeySequence(QStringLiteral("Ctrl+2")));
    sm.registerAction(QStringLiteral("view.focusMedia"), m_actFocusMedia,
                      QKeySequence(QStringLiteral("Ctrl+I")));
    sm.registerAction(QStringLiteral("view.focusEffects"), m_actFocusEffects,
                      QKeySequence(QStringLiteral("Ctrl+E")));
    sm.registerAction(QStringLiteral("view.focusProperties"), m_actFocusProperties,
                      QKeySequence(QStringLiteral("Ctrl+P")));
    sm.registerAction(QStringLiteral("view.announceContext"),
                      m_actAnnounceContext,
                      QKeySequence(QStringLiteral("Ctrl+Shift+W")));
    sm.registerAction(QStringLiteral("view.cycleContextVerbosity"),
                      m_actCycleContextVerbosity,
                      QKeySequence(QStringLiteral("Ctrl+Shift+V")));
    sm.registerAction(QStringLiteral("help.announceKeyboardHelp"),
                      m_actAnnounceShortcuts,
                      QKeySequence(QStringLiteral("Ctrl+Shift+H")));
    sm.registerAction(QStringLiteral("timeline.toggleMarkerJumpSnap"),
                      m_actToggleMarkerJumpSnap,
                      QKeySequence(QStringLiteral("Ctrl+Shift+J")));

    // Track reorder
    sm.registerAction(QStringLiteral("timeline.moveTrackUp"), m_actMoveTrackUp,
                      QKeySequence(QStringLiteral("Alt+Up")));
    sm.registerAction(QStringLiteral("timeline.moveTrackDown"), m_actMoveTrackDown,
                      QKeySequence(QStringLiteral("Alt+Down")));

    // Phase 7 additions
    sm.registerAction(QStringLiteral("timeline.removeMarker"), m_actRemoveMarker,
                      QKeySequence(QStringLiteral("Ctrl+Shift+M")));
    sm.registerAction(QStringLiteral("timeline.moveClipUp"), m_actMoveClipUp,
                      QKeySequence(QStringLiteral("Shift+Up")));
    sm.registerAction(QStringLiteral("timeline.moveClipDown"), m_actMoveClipDown,
                      QKeySequence(QStringLiteral("Shift+Down")));
    sm.registerAction(QStringLiteral("transport.goToTimecode"), m_actGoToTimecode,
                      QKeySequence(QStringLiteral("Ctrl+G")));
    sm.registerAction(QStringLiteral("timeline.soloTrack"), m_actSoloTrack,
                      QKeySequence(QStringLiteral("Ctrl+Shift+O")));

    // Nudge clip position
    sm.registerAction(QStringLiteral("timeline.nudgeClipLeft"), m_actNudgeClipLeft,
                      QKeySequence(QStringLiteral("Ctrl+Left")));
    sm.registerAction(QStringLiteral("timeline.nudgeClipRight"), m_actNudgeClipRight,
                      QKeySequence(QStringLiteral("Ctrl+Right")));
}

void MainWindow::rebuildTractor()
{
    m_tractorBuilder->rebuild(m_project->timeline());
    m_timeline->refresh();
}

void MainWindow::deferRebuildTractor()
{
    // Restart the debounce timer.  If multiple calls arrive within
    // 100 ms the tractor is only rebuilt once – preventing the
    // consumer close/open race that crashes on rapid clip additions.
    m_rebuildTimer->start();
}

void MainWindow::reconnectTimeline()
{
    auto *tl = m_project->timeline();

    // Reconnect the tracksChanged signal to rebuild the MLT pipeline
    connect(tl, &Timeline::tracksChanged,
            this, &MainWindow::deferRebuildTractor);

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

void MainWindow::addRecentProject(const QString &path)
{
    QSettings s;
    QStringList recent = s.value(QStringLiteral("recentProjects")).toStringList();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > 10)
        recent.removeLast();
    s.setValue(QStringLiteral("recentProjects"), recent);
    updateRecentMenu();
}

void MainWindow::updateRecentMenu()
{
    if (!m_recentMenu) return;
    m_recentMenu->clear();

    QSettings s;
    const QStringList recent =
        s.value(QStringLiteral("recentProjects")).toStringList();

    if (recent.isEmpty()) {
        auto *empty = m_recentMenu->addAction(tr("(none)"));
        empty->setEnabled(false);
        return;
    }

    for (const QString &filePath : recent) {
        const QString label = QFileInfo(filePath).fileName();
        auto *act = m_recentMenu->addAction(label);
        act->setData(filePath);
        connect(act, &QAction::triggered, this, [this, filePath]() {
            if (!QFileInfo::exists(filePath)) {
                m_announcer->announce(
                    tr("File not found: %1").arg(filePath),
                    Announcer::Priority::High);
                return;
            }
            m_playback->close();
            if (!m_serializer->load(m_project, filePath)) {
                QMessageBox::warning(this, tr("Open Failed"),
                                     m_serializer->lastError());
                return;
            }
            m_currentFilePath = filePath;
            m_modified = false;
            m_undoStack->clear();
            reconnectTimeline();
            rebuildTractor();
            m_timeline->refresh();
            updateWindowTitle();
            addRecentProject(filePath);
            m_announcer->announce(
                tr("Project opened: %1").arg(filePath),
                Announcer::Priority::High);
        });
    }
}

void MainWindow::setupAutoSave()
{
    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setInterval(3 * 60 * 1000); // 3 minutes
    connect(m_autoSaveTimer, &QTimer::timeout,
            this, &MainWindow::performAutoSave);
    m_autoSaveTimer->start();
}

void MainWindow::performAutoSave()
{
    if (!m_modified && !m_project->isModified())
        return;

    QString autoPath;
    if (!m_currentFilePath.isEmpty()) {
        autoPath = m_currentFilePath + QStringLiteral(".autosave");
    } else {
        const QString dir = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation);
        QDir().mkpath(dir);
        autoPath = dir + QStringLiteral("/autosave.tvs");
    }

    m_serializer->setMltXml(m_tractorBuilder->serializeToXml());
    if (m_serializer->save(m_project, autoPath)) {
        // Silent success – don't announce to avoid interrupting work
    } else {
        m_announcer->announce(
            tr("Auto-save failed."), Announcer::Priority::Low);
    }
}

void MainWindow::removeAutoSaveFile()
{
    // Remove project-specific auto-save
    if (!m_currentFilePath.isEmpty()) {
        const QString autoPath =
            m_currentFilePath + QStringLiteral(".autosave");
        QFile::remove(autoPath);
    }

    // Also remove the fallback generic auto-save
    const QString dir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    QFile::remove(dir + QStringLiteral("/autosave.tvs"));
}

void MainWindow::checkAutoSaveRecovery()
{
    // Check for a generic (untitled project) auto-save
    const QString appDir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    const QString genericAutoSave =
        appDir + QStringLiteral("/autosave.tvs");

    if (QFileInfo::exists(genericAutoSave)) {
        auto answer = QMessageBox::question(
            this, tr("Recover Auto-Save?"),
            tr("An auto-saved project was found from a previous session.\n"
               "Would you like to recover it?"),
            QMessageBox::Yes | QMessageBox::No);

        if (answer == QMessageBox::Yes) {
            m_playback->close();
            if (m_serializer->load(m_project, genericAutoSave)) {
                m_currentFilePath.clear(); // not a real saved file
                m_modified = true;
                m_undoStack->clear();
                reconnectTimeline();
                rebuildTractor();
                m_timeline->refresh();
                updateWindowTitle();
                m_announcer->announce(
                    tr("Auto-saved project recovered."),
                    Announcer::Priority::High);
            } else {
                m_announcer->announce(
                    tr("Failed to recover auto-save."),
                    Announcer::Priority::High);
            }
        }

        // Remove auto-save regardless of choice
        QFile::remove(genericAutoSave);
    }
}

} // namespace Thrive
