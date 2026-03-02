// SPDX-License-Identifier: MIT
// Thrive Video Suite – Properties panel implementation

#include "propertiespanel.h"
#include "../core/clip.h"
#include "../core/track.h"
#include "../core/timecode.h"
#include "../accessibility/announcer.h"

#include <QFormLayout>
#include <QLineEdit>
#include <QLabel>
#include <QTextEdit>
#include <QScrollArea>
#include <QVBoxLayout>

namespace Thrive {

PropertiesPanel::PropertiesPanel(Announcer *announcer, QWidget *parent)
    : QWidget(parent)
    , m_announcer(announcer)
{
    setObjectName(QStringLiteral("PropertiesPanel"));
    setAccessibleName(tr("Properties"));

    auto *outerLayout = new QVBoxLayout(this);
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setAccessibleName(tr("Property fields"));
    outerLayout->addWidget(m_scrollArea);

    m_formWidget = new QWidget;
    m_formLayout = new QFormLayout(m_formWidget);
    m_scrollArea->setWidget(m_formWidget);

    buildClipForm();
    clear();
}

void PropertiesPanel::buildClipForm()
{
    m_clipName = new QLineEdit(m_formWidget);
    m_clipName->setAccessibleName(tr("Clip name"));
    m_formLayout->addRow(tr("&Name:"), m_clipName);

    m_clipDescription = new QTextEdit(m_formWidget);
    m_clipDescription->setAccessibleName(tr("Clip description"));
    m_clipDescription->setMaximumHeight(80);
    m_formLayout->addRow(tr("&Description:"), m_clipDescription);

    m_clipSource = new QLabel(m_formWidget);
    m_clipSource->setAccessibleName(tr("Source file"));
    m_formLayout->addRow(tr("Source:"), m_clipSource);

    m_clipInPoint = new QLabel(m_formWidget);
    m_clipInPoint->setAccessibleName(tr("In point"));
    m_formLayout->addRow(tr("In:"), m_clipInPoint);

    m_clipOutPoint = new QLabel(m_formWidget);
    m_clipOutPoint->setAccessibleName(tr("Out point"));
    m_formLayout->addRow(tr("Out:"), m_clipOutPoint);

    m_clipDuration = new QLabel(m_formWidget);
    m_clipDuration->setAccessibleName(tr("Duration"));
    m_formLayout->addRow(tr("Duration:"), m_clipDuration);

    connect(m_clipName, &QLineEdit::editingFinished,
            this, &PropertiesPanel::onClipNameEdited);
    connect(m_clipDescription, &QTextEdit::textChanged,
            this, &PropertiesPanel::onClipDescriptionEdited);
}

void PropertiesPanel::inspectClip(Clip *clip)
{
    m_currentClip  = clip;
    m_currentTrack = nullptr;

    if (!clip) {
        clear();
        return;
    }

    m_formWidget->setVisible(true);

    m_clipName->setText(clip->name());
    m_clipDescription->setPlainText(clip->description());
    m_clipSource->setText(clip->sourcePath());
    m_clipInPoint->setText(clip->inPoint().toString());
    m_clipOutPoint->setText(clip->outPoint().toString());

    const int64_t durationFrames = clip->outPoint().frame()
                                 - clip->inPoint().frame();
    TimeCode dur(durationFrames, clip->inPoint().fps());
    m_clipDuration->setText(dur.toString()
                            + QStringLiteral(" (")
                            + dur.toSpokenString()
                            + QStringLiteral(")"));

    m_announcer->announce(
        tr("Inspecting clip: %1").arg(clip->name()),
        Announcer::Priority::Low);
}

void PropertiesPanel::inspectTrack(Track *track)
{
    m_currentClip  = nullptr;
    m_currentTrack = track;

    if (!track) {
        clear();
        return;
    }

    // For now re-use clip form fields to show track info
    m_formWidget->setVisible(true);
    m_clipName->setText(track->name());
    m_clipDescription->clear();
    m_clipSource->setText(track->type() == Track::Type::Video
                              ? tr("Video track") : tr("Audio track"));
    m_clipInPoint->clear();
    m_clipOutPoint->clear();
    m_clipDuration->setText(tr("%n clip(s)", nullptr, track->clips().size()));

    m_announcer->announce(
        tr("Inspecting track: %1").arg(track->name()),
        Announcer::Priority::Low);
}

void PropertiesPanel::clear()
{
    m_currentClip  = nullptr;
    m_currentTrack = nullptr;
    m_clipName->clear();
    m_clipDescription->clear();
    m_clipSource->clear();
    m_clipInPoint->clear();
    m_clipOutPoint->clear();
    m_clipDuration->clear();
    m_formWidget->setVisible(false);
}

void PropertiesPanel::onClipNameEdited()
{
    if (m_currentClip) {
        m_currentClip->setName(m_clipName->text());
    }
}

void PropertiesPanel::onClipDescriptionEdited()
{
    if (m_currentClip) {
        m_currentClip->setDescription(
            m_clipDescription->toPlainText());
    }
}

} // namespace Thrive
