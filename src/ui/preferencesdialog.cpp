// SPDX-License-Identifier: MIT
// Thrive Video Suite – Preferences dialog implementation

#include "preferencesdialog.h"
#include "shortcuteditor.h"
#include "pluginmanager.h"
#include "../core/project.h"
#include "../accessibility/announcer.h"

#include <QTabWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QSettings>

namespace Thrive {

PreferencesDialog::PreferencesDialog(Project *project,
                                     Announcer *announcer,
                                     QWidget *parent)
    : QDialog(parent)
    , m_project(project)
    , m_announcer(announcer)
{
    setWindowTitle(tr("Preferences"));
    setAccessibleDescription(tr("Application settings"));
    setMinimumSize(600, 450);

    auto *mainLayout = new QVBoxLayout(this);

    m_tabs = new QTabWidget(this);
    m_tabs->setAccessibleName(tr("Preference categories"));

    // ── General tab ─────────────────────────────────────────────────
    m_tabs->addTab(createGeneralTab(), tr("&General"));

    // ── Shortcuts tab ───────────────────────────────────────────────
    m_shortcutEditor = new ShortcutEditor(m_announcer, this);
    m_tabs->addTab(m_shortcutEditor, tr("&Shortcuts"));

    // ── Plugins tab ─────────────────────────────────────────────────
    m_pluginManager = new PluginManager(m_announcer, this);
    m_tabs->addTab(m_pluginManager, tr("&Plugins"));

    connect(m_pluginManager, &PluginManager::restartRequired,
            this, &PreferencesDialog::restartRequired);

    // ── Buttons ─────────────────────────────────────────────────────
    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_buttonBox, &QDialogButtonBox::accepted,
            this, &PreferencesDialog::onAccepted);
    connect(m_buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);

    mainLayout->addWidget(m_tabs, 1);
    mainLayout->addWidget(m_buttonBox);
}

QWidget *PreferencesDialog::createGeneralTab()
{
    auto *page   = new QWidget;
    auto *form   = new QFormLayout(page);

    // Preview resolution
    m_previewScale = new QComboBox(page);
    m_previewScale->setAccessibleName(tr("Preview resolution"));
    m_previewScale->addItem(tr("360p"),  360);
    m_previewScale->addItem(tr("640p"),  640);
    m_previewScale->addItem(tr("720p"),  720);
    // Select current
    int idx = m_previewScale->findData(m_project->previewScale());
    if (idx >= 0) m_previewScale->setCurrentIndex(idx);
    form->addRow(tr("Preview &resolution:"), m_previewScale);

    // Scrub audio toggle
    m_scrubAudio = new QCheckBox(tr("Play audio when scrubbing"), page);
    m_scrubAudio->setAccessibleName(tr("Enable scrub audio"));
    m_scrubAudio->setChecked(m_project->scrubAudioEnabled());
    form->addRow(m_scrubAudio);

    // Audio cues toggle
    m_audioCuesEnabled = new QCheckBox(tr("Enable navigation audio cues"), page);
    m_audioCuesEnabled->setAccessibleName(tr("Enable audio cues"));
    {
        QSettings settings;
        m_audioCuesEnabled->setChecked(
            settings.value(QStringLiteral("audioCues/enabled"), true).toBool());
    }
    form->addRow(m_audioCuesEnabled);

    // Audio cue volume
    m_audioCueVolume = new QSlider(Qt::Horizontal, page);
    m_audioCueVolume->setAccessibleName(tr("Audio cue volume"));
    m_audioCueVolume->setRange(0, 100);
    {
        QSettings settings;
        m_audioCueVolume->setValue(
            settings.value(QStringLiteral("audioCues/volume"), 50).toInt());
    }
    form->addRow(tr("Cue &volume:"), m_audioCueVolume);

    return page;
}

void PreferencesDialog::onAccepted()
{
    // Apply general settings
    const int scale = m_previewScale->currentData().toInt();
    if (scale != m_project->previewScale()) {
        m_project->setPreviewScale(scale);
        emit previewScaleChanged(scale);
    }

    m_project->setScrubAudioEnabled(m_scrubAudio->isChecked());

    emit audioCuesEnabledChanged(m_audioCuesEnabled->isChecked());
    emit audioCueVolumeChanged(
        static_cast<float>(m_audioCueVolume->value()) / 100.0f);

    // Persist audio cue settings so they survive restart
    QSettings settings;
    settings.setValue(QStringLiteral("audioCues/enabled"),
                      m_audioCuesEnabled->isChecked());
    settings.setValue(QStringLiteral("audioCues/volume"),
                      m_audioCueVolume->value());

    accept();
}

} // namespace Thrive
