// SPDX-License-Identifier: MIT
// Thrive Video Suite – Main window

#pragma once

#include <QMainWindow>
#include <QUndoStack>

namespace Thrive {

class Project;
class MltEngine;
class PlaybackController;
class RenderEngine;
class EffectCatalog;
class Announcer;
class AudioCueManager;
class ProjectSerializer;

class TimelineWidget;
class TransportBar;
class MediaBrowser;
class PropertiesPanel;
class EffectsBrowser;

/// The main application window.  Assembles all panels as QDockWidgets
/// and wires up top-level actions.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(Project *project,
                        MltEngine *engine,
                        Announcer *announcer,
                        AudioCueManager *cues,
                        QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void newProject();
    void openProject();
    void saveProject();
    void saveProjectAs();
    void exportVideo();
    void showPreferences();
    void showWelcomeWizard();
    void showAbout();

private:
    void createActions();
    void createMenus();
    void createDockWidgets();
    void registerShortcuts();
    void checkFirstRun();
    void checkPostRestartPlugins();

    // Owned subsystems
    Project            *m_project   = nullptr;
    MltEngine          *m_engine    = nullptr;
    PlaybackController *m_playback  = nullptr;
    RenderEngine       *m_render    = nullptr;
    EffectCatalog      *m_catalog   = nullptr;
    Announcer          *m_announcer = nullptr;
    AudioCueManager    *m_cues      = nullptr;
    QUndoStack         *m_undoStack = nullptr;
    ProjectSerializer  *m_serializer = nullptr;

    // UI panels
    TimelineWidget   *m_timeline   = nullptr;
    TransportBar     *m_transport  = nullptr;
    MediaBrowser     *m_media      = nullptr;
    PropertiesPanel  *m_properties = nullptr;
    EffectsBrowser   *m_effects    = nullptr;

    // File state
    QString m_currentFilePath;
    bool    m_modified = false;

    // Actions
    QAction *m_actNew         = nullptr;
    QAction *m_actOpen        = nullptr;
    QAction *m_actSave        = nullptr;
    QAction *m_actSaveAs      = nullptr;
    QAction *m_actExport      = nullptr;
    QAction *m_actPreferences = nullptr;
    QAction *m_actWizard      = nullptr;
    QAction *m_actAbout       = nullptr;
    QAction *m_actQuit        = nullptr;
    QAction *m_actUndo        = nullptr;
    QAction *m_actRedo        = nullptr;
};

} // namespace Thrive
