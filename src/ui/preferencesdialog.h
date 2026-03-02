// SPDX-License-Identifier: MIT
// Thrive Video Suite – Preferences dialog

#pragma once

#include <QDialog>

QT_FORWARD_DECLARE_CLASS(QTabWidget)
QT_FORWARD_DECLARE_CLASS(QComboBox)
QT_FORWARD_DECLARE_CLASS(QCheckBox)
QT_FORWARD_DECLARE_CLASS(QSlider)
QT_FORWARD_DECLARE_CLASS(QDialogButtonBox)

namespace Thrive {

class ShortcutEditor;
class PluginManager;
class Announcer;
class Project;

/// Application preferences with tabs for General, Shortcuts, and Plugins.
class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(Project *project,
                               Announcer *announcer,
                               QWidget *parent = nullptr);

    ShortcutEditor *shortcutEditor() const { return m_shortcutEditor; }
    PluginManager  *pluginManager()  const { return m_pluginManager; }

signals:
    void previewScaleChanged(int heightPixels);
    void audioCuesEnabledChanged(bool enabled);
    void audioCueVolumeChanged(float volume);
    void restartRequired();

private slots:
    void onAccepted();

private:
    QWidget *createGeneralTab();

    Project    *m_project    = nullptr;
    Announcer  *m_announcer = nullptr;

    QTabWidget       *m_tabs       = nullptr;
    QDialogButtonBox *m_buttonBox  = nullptr;

    // General tab widgets
    QComboBox *m_previewScale     = nullptr;
    QCheckBox *m_scrubAudio       = nullptr;
    QCheckBox *m_audioCuesEnabled = nullptr;
    QSlider   *m_audioCueVolume   = nullptr;

    // Embedded widgets
    ShortcutEditor *m_shortcutEditor = nullptr;
    PluginManager  *m_pluginManager  = nullptr;
};

} // namespace Thrive
