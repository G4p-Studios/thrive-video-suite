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
#include <QApplication>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QAbstractButton>
#include <QComboBox>
#include <QAbstractSpinBox>

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

    // Safely handle timeline replacement (e.g. Project::reset)
    connect(m_project, &Project::timelineAboutToChange,
            this, [this]() {
                // Disconnect from the old timeline before it is deleted
                m_project->timeline()->disconnect(this);
            });

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
                       qobject_cast<QLineEdit *>(fw))) {
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

    // Pause playback before rendering
    m_playback->pause();

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
        m_clipboardClip = new Clip(src->name(), src->sourcePath(),
                                   src->inPoint(), src->outPoint(), this);
        m_clipboardClip->setTimelinePosition(src->timelinePosition());
        m_clipboardClip->setDescription(src->description());
        for (auto *fx : src->effects()) {
            auto *fxCopy = new Effect(fx->serviceId(), fx->displayName(),
                                      fx->description(), m_clipboardClip);
            fxCopy->setEnabled(fx->isEnabled());
            for (const auto &p : fx->parameters()) {
                fxCopy->addParameter(p);
            }
            m_clipboardClip->addEffect(fxCopy);
        }
        if (auto *t = src->outTransition()) {
            m_clipboardClip->setOutTransition(
                new Transition(t->serviceId(), t->displayName(),
                               t->description(), t->duration(),
                               m_clipboardClip));
        }
        if (auto *t = src->inTransition()) {
            m_clipboardClip->setInTransition(
                new Transition(t->serviceId(), t->displayName(),
                               t->description(), t->duration(),
                               m_clipboardClip));
        }

        m_undoStack->push(new RemoveClipCommand(trk, idx));
        m_modified = true;
        rebuildTractor();
        m_announcer->announce(
            tr("Cut: %1").arg(clipName), Announcer::Priority::Normal);
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
        m_clipboardClip = new Clip(src->name(), src->sourcePath(),
                                   src->inPoint(), src->outPoint(), this);
        m_clipboardClip->setTimelinePosition(src->timelinePosition());
        m_clipboardClip->setDescription(src->description());
        for (auto *fx : src->effects()) {
            auto *fxCopy = new Effect(fx->serviceId(), fx->displayName(),
                                      fx->description(), m_clipboardClip);
            fxCopy->setEnabled(fx->isEnabled());
            for (const auto &p : fx->parameters()) {
                fxCopy->addParameter(p);
            }
            m_clipboardClip->addEffect(fxCopy);
        }
        if (auto *t = src->outTransition()) {
            m_clipboardClip->setOutTransition(
                new Transition(t->serviceId(), t->displayName(),
                               t->description(), t->duration(),
                               m_clipboardClip));
        }
        if (auto *t = src->inTransition()) {
            m_clipboardClip->setInTransition(
                new Transition(t->serviceId(), t->displayName(),
                               t->description(), t->duration(),
                               m_clipboardClip));
        }

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
        auto *newClip = new Clip(m_clipboardClip->name(),
                                 m_clipboardClip->sourcePath(),
                                 m_clipboardClip->inPoint(),
                                 m_clipboardClip->outPoint());
        newClip->setTimelinePosition(tl->playheadPosition());
        newClip->setDescription(m_clipboardClip->description());
        for (auto *fx : m_clipboardClip->effects()) {
            auto *fxCopy = new Effect(fx->serviceId(), fx->displayName(),
                                      fx->description(), newClip);
            fxCopy->setEnabled(fx->isEnabled());
            for (const auto &p : fx->parameters()) {
                fxCopy->addParameter(p);
            }
            newClip->addEffect(fxCopy);
        }
        if (auto *t = m_clipboardClip->outTransition()) {
            newClip->setOutTransition(
                new Transition(t->serviceId(), t->displayName(),
                               t->description(), t->duration(), newClip));
        }
        if (auto *t = m_clipboardClip->inTransition()) {
            newClip->setInTransition(
                new Transition(t->serviceId(), t->displayName(),
                               t->description(), t->duration(), newClip));
        }

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
        rebuildTractor();
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
    });
    addAction(m_actFrameBack);

    m_actFrameForward = new QAction(tr("Step Frame Forward"), this);
    m_actFrameForward->setShortcut(Qt::Key_Period);
    connect(m_actFrameForward, &QAction::triggered, this, [this]() {
        m_playback->stepFrames(1);
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

        if (alreadySoloed) {
            // Un-solo: unmute all tracks
            for (int i = 0; i < tl->trackCount(); ++i)
                tl->trackAt(i)->setMuted(false);
            m_announcer->announce(
                tr("All tracks unmuted."), Announcer::Priority::High);
        } else {
            // Solo: mute everything except current
            for (int i = 0; i < tl->trackCount(); ++i)
                tl->trackAt(i)->setMuted(i != tl->currentTrackIndex());
            m_announcer->announce(
                tr("%1 soloed.").arg(cur->name()),
                Announcer::Priority::High);
        }
        m_modified = true;
        rebuildTractor();
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
    timelineMenu->addSeparator();
    timelineMenu->addAction(m_actAddMarker);
    timelineMenu->addAction(m_actRemoveMarker);
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
    viewMenu->addAction(m_actFocusMedia);
    viewMenu->addAction(m_actFocusEffects);
    viewMenu->addAction(m_actFocusProperties);

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
    transportDock->setAccessibleDescription(
        tr("Transport controls. Space to play/pause, J to rewind, L to fast forward."));
    addDockWidget(Qt::BottomDockWidgetArea, transportDock);

    // Media browser (left)
    m_media = new MediaBrowser(m_announcer, m_engine, this);
    auto *mediaDock = new QDockWidget(tr("Media"), this);
    mediaDock->setWidget(m_media);
    mediaDock->setAccessibleDescription(
        tr("Media browser. Browse and import media files into your project."));
    addDockWidget(Qt::LeftDockWidgetArea, mediaDock);

    // Properties panel (right)
    m_properties = new PropertiesPanel(m_announcer, m_undoStack, this);
    auto *propDock = new QDockWidget(tr("Properties"), this);
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
    effectsDock->setWidget(m_effects);
    effectsDock->setAccessibleDescription(
        tr("Effects browser. Browse and apply audio and video effects."));
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

                // Add to the currently selected track
                int trkIdx = tl->currentTrackIndex();
                auto *trk = tl->trackAt(trkIdx);
                if (!trk) {
                    // Fallback to track 0
                    trk = tl->trackAt(0);
                }
                if (!trk) return;

                m_undoStack->push(new AddClipCommand(trk, clip));
                m_modified = true;
                rebuildTractor();

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
    sm.registerAction(QStringLiteral("view.focusMedia"), m_actFocusMedia,
                      QKeySequence(QStringLiteral("Ctrl+I")));
    sm.registerAction(QStringLiteral("view.focusEffects"), m_actFocusEffects,
                      QKeySequence(QStringLiteral("Ctrl+E")));
    sm.registerAction(QStringLiteral("view.focusProperties"), m_actFocusProperties,
                      QKeySequence(QStringLiteral("Ctrl+P")));

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
