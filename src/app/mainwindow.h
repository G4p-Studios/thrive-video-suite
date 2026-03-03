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
class TractorBuilder;
class Announcer;
class AudioCueManager;
class ProjectSerializer;

class TimelineWidget;
class TransportBar;
class MediaBrowser;
class PropertiesPanel;
class EffectsBrowser;
class Clip;

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
    void rebuildTractor();
    void reconnectTimeline();
    void updateWindowTitle();
    void addRecentProject(const QString &path);
    void updateRecentMenu();

    // Owned subsystems
    Project            *m_project   = nullptr;
    MltEngine          *m_engine    = nullptr;
    PlaybackController *m_playback  = nullptr;
    RenderEngine       *m_render    = nullptr;
    EffectCatalog      *m_catalog   = nullptr;
    TractorBuilder     *m_tractorBuilder = nullptr;
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

    // Clipboard for cut/copy/paste
    Clip   *m_clipboardClip = nullptr;

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
    QAction *m_actCut         = nullptr;
    QAction *m_actCopy        = nullptr;
    QAction *m_actPaste       = nullptr;
    QAction *m_actDelete      = nullptr;
    QAction *m_actSelectAll   = nullptr;

    // Timeline menu actions
    QAction *m_actSplitClip      = nullptr;
    QAction *m_actAddTrack       = nullptr;
    QAction *m_actRemoveTrack    = nullptr;
    QAction *m_actAddMarker      = nullptr;
    QAction *m_actAddTransition  = nullptr;
    QAction *m_actMuteTrack      = nullptr;
    QAction *m_actLockTrack      = nullptr;

    // Transport actions (global shortcuts)
    QAction *m_actPlayPause      = nullptr;
    QAction *m_actJRewind        = nullptr;
    QAction *m_actLForward       = nullptr;
    QAction *m_actFrameBack      = nullptr;
    QAction *m_actFrameForward   = nullptr;

    // Quick-access actions
    QAction *m_actFocusMedia     = nullptr;
    QAction *m_actFocusEffects   = nullptr;

    // Track reorder actions
    QAction *m_actMoveTrackUp    = nullptr;
    QAction *m_actMoveTrackDown  = nullptr;

    // Recent projects
    QMenu   *m_recentMenu        = nullptr;
};

} // namespace Thrive
