// SPDX-License-Identifier: MIT
// Thrive Video Suite – Main window implementation

#include "mainwindow.h"
#include "constants.h"
#include "shortcutmanager.h"
#include "welcomewizard.h"

#include "../core/project.h"
#include "../core/timeline.h"
#include "../core/track.h"
#include "../core/projectserializer.h"
#include "../engine/mltengine.h"
#include "../engine/playbackcontroller.h"
#include "../engine/renderengine.h"
#include "../engine/effectcatalog.h"
#include "../accessibility/announcer.h"
#include "../accessibility/audiocuemanager.h"

#include "../ui/timelinewidget.h"
#include "../ui/transportbar.h"
#include "../ui/mediabrowser.h"
#include "../ui/propertiespanel.h"
#include "../ui/effectsbrowser.h"
#include "../ui/preferencesdialog.h"
#include "../ui/shortcuteditor.h"

#include <QAction>
#include <QMenuBar>
#include <QMenu>
#include <QDockWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QSettings>
#include <QApplication>

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
    m_playback = new PlaybackController(m_engine, this);
    m_render   = new RenderEngine(m_engine, this);
    m_catalog  = new EffectCatalog(m_engine, this);
    m_catalog->refresh();

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
    m_project->reset();
    m_currentFilePath.clear();
    m_modified = false;
    m_timeline->refresh();
    m_announcer->announce(tr("New project created."),
                          Announcer::Priority::High);
}

void MainWindow::openProject()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Project"), QString(),
        tr("TVS Projects (*.tvs);;All Files (*)"));
    if (path.isEmpty()) return;

    if (!m_serializer->load(m_project, path)) {
        QMessageBox::warning(this, tr("Open Failed"),
                             m_serializer->lastError());
        return;
    }

    m_currentFilePath = path;
    m_modified = false;
    m_timeline->refresh();
    m_announcer->announce(
        tr("Project opened: %1").arg(path), Announcer::Priority::High);
}

void MainWindow::saveProject()
{
    if (m_currentFilePath.isEmpty()) {
        saveProjectAs();
        return;
    }

    if (!m_serializer->save(m_project, m_currentFilePath)) {
        QMessageBox::warning(this, tr("Save Failed"),
                             m_serializer->lastError());
        return;
    }

    m_modified = false;
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

    // TODO: pass the actual Mlt::Tractor from engine once timeline is wired
    m_render->startRender(nullptr, path,
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

    // Edit action stubs – these will be wired to timeline clip operations
    connect(m_actCut, &QAction::triggered, this, [this]() {
        m_announcer->announce(tr("Cut"), Announcer::Priority::Normal);
    });
    connect(m_actCopy, &QAction::triggered, this, [this]() {
        m_announcer->announce(tr("Copy"), Announcer::Priority::Normal);
    });
    connect(m_actPaste, &QAction::triggered, this, [this]() {
        m_announcer->announce(tr("Paste"), Announcer::Priority::Normal);
    });
    connect(m_actDelete, &QAction::triggered, this, [this]() {
        m_announcer->announce(tr("Delete"), Announcer::Priority::Normal);
    });
    connect(m_actSelectAll, &QAction::triggered, this, [this]() {
        m_announcer->announce(tr("Select all"), Announcer::Priority::Normal);
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
    m_properties = new PropertiesPanel(m_announcer, this);
    auto *propDock = new QDockWidget(tr("Properties"), this);
    propDock->setWidget(m_properties);
    addDockWidget(Qt::RightDockWidgetArea, propDock);

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
