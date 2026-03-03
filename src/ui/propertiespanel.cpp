// SPDX-License-Identifier: MIT
// Thrive Video Suite – Properties panel implementation

#include "propertiespanel.h"
#include "../core/clip.h"
#include "../core/track.h"
#include "../core/effect.h"
#include "../core/timecode.h"
#include "../core/commands.h"
#include "../accessibility/announcer.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QSpinBox>
#include <QTextEdit>
#include <QUndoStack>
#include <QVBoxLayout>

namespace Thrive {

PropertiesPanel::PropertiesPanel(Announcer *announcer,
                                 QUndoStack *undoStack,
                                 QWidget *parent)
    : QWidget(parent)
    , m_announcer(announcer)
    , m_undoStack(undoStack)
{
    setObjectName(QStringLiteral("PropertiesPanel"));
    setAccessibleName(tr("Properties"));

    auto *outerLayout = new QVBoxLayout(this);
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setAccessibleName(tr("Property fields"));
    outerLayout->addWidget(m_scrollArea);

    m_formWidget = new QWidget;
    auto *mainLayout = new QVBoxLayout(m_formWidget);

    // ── Core property form ───────────────────────────────────────────
    m_formLayout = new QFormLayout;
    mainLayout->addLayout(m_formLayout);

    buildClipForm();

    // ── Effects section ──────────────────────────────────────────────
    buildEffectsSection();
    mainLayout->addWidget(m_effectsGroup);
    mainLayout->addStretch();

    m_scrollArea->setWidget(m_formWidget);

    clear();
}

// ── form construction ────────────────────────────────────────────────

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

    m_clipInPoint = new QLineEdit(m_formWidget);
    m_clipInPoint->setAccessibleName(tr("In point timecode"));
    m_clipInPoint->setPlaceholderText(QStringLiteral("00:00:00:00"));
    m_formLayout->addRow(tr("&In:"), m_clipInPoint);

    m_clipOutPoint = new QLineEdit(m_formWidget);
    m_clipOutPoint->setAccessibleName(tr("Out point timecode"));
    m_clipOutPoint->setPlaceholderText(QStringLiteral("00:00:00:00"));
    m_formLayout->addRow(tr("&Out:"), m_clipOutPoint);

    m_clipDuration = new QLabel(m_formWidget);
    m_clipDuration->setAccessibleName(tr("Duration"));
    m_formLayout->addRow(tr("Duration:"), m_clipDuration);

    connect(m_clipName, &QLineEdit::editingFinished,
            this, &PropertiesPanel::onClipNameEdited);
    connect(m_clipDescription, &QTextEdit::textChanged,
            this, &PropertiesPanel::onClipDescriptionEdited);
    connect(m_clipInPoint, &QLineEdit::editingFinished,
            this, &PropertiesPanel::onInPointEdited);
    connect(m_clipOutPoint, &QLineEdit::editingFinished,
            this, &PropertiesPanel::onOutPointEdited);
}

void PropertiesPanel::buildEffectsSection()
{
    m_effectsGroup = new QGroupBox(tr("Effects"), m_formWidget);
    m_effectsGroup->setAccessibleName(tr("Clip effects"));
    m_effectsLayout = new QVBoxLayout(m_effectsGroup);
    m_effectsGroup->setVisible(false);
}

// ── inspect ──────────────────────────────────────────────────────────

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
    m_clipSource->setVisible(true);

    m_clipInPoint->setText(clip->inPoint().toString());
    m_clipInPoint->setEnabled(true);
    m_clipOutPoint->setText(clip->outPoint().toString());
    m_clipOutPoint->setEnabled(true);

    const int64_t durationFrames = clip->outPoint().frame()
                                 - clip->inPoint().frame();
    TimeCode dur(durationFrames, clip->inPoint().fps());
    m_clipDuration->setText(dur.toString()
                            + QStringLiteral(" (")
                            + dur.toSpokenString()
                            + QStringLiteral(")"));

    // Populate effects
    populateEffects(clip->effects());

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

    m_formWidget->setVisible(true);
    m_clipName->setText(track->name());
    m_clipDescription->clear();
    m_clipSource->setText(track->type() == Track::Type::Video
                              ? tr("Video track") : tr("Audio track"));
    m_clipSource->setVisible(true);

    m_clipInPoint->clear();
    m_clipInPoint->setEnabled(false);
    m_clipOutPoint->clear();
    m_clipOutPoint->setEnabled(false);
    m_clipDuration->setText(tr("%n clip(s)", nullptr, track->clips().size()));

    clearEffects();

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
    m_clipInPoint->setEnabled(false);
    m_clipOutPoint->clear();
    m_clipOutPoint->setEnabled(false);
    m_clipDuration->clear();
    clearEffects();
    m_formWidget->setVisible(false);
}

// ── slots ────────────────────────────────────────────────────────────

void PropertiesPanel::onClipNameEdited()
{
    if (m_currentClip) {
        const QString newName = m_clipName->text();
        if (newName != m_currentClip->name())
            m_undoStack->push(new RenameClipCommand(m_currentClip, newName));
    } else if (m_currentTrack) {
        m_currentTrack->setName(m_clipName->text());
    }
}

void PropertiesPanel::onClipDescriptionEdited()
{
    if (m_currentClip) {
        const QString newDesc = m_clipDescription->toPlainText();
        if (newDesc != m_currentClip->description())
            m_undoStack->push(
                new ChangeClipDescriptionCommand(m_currentClip, newDesc));
    }
}

void PropertiesPanel::onInPointEdited()
{
    if (!m_currentClip)
        return;

    const double fps = m_currentClip->inPoint().fps();
    const TimeCode newIn = TimeCode::fromString(m_clipInPoint->text(), fps);

    if (newIn == m_currentClip->inPoint())
        return; // unchanged

    if (newIn.frame() < 0 || newIn >= m_currentClip->outPoint()) {
        // Revert to current value
        m_clipInPoint->setText(m_currentClip->inPoint().toString());
        m_announcer->announce(
            tr("Invalid in point. Must be before the out point."),
            Announcer::Priority::Normal);
        return;
    }

    m_undoStack->push(new TrimClipCommand(
        m_currentClip, TrimClipCommand::Edge::In, newIn));
    emit clipTrimmed();

    // Refresh duration display
    const int64_t dur = m_currentClip->outPoint().frame() - newIn.frame();
    TimeCode durTc(dur, fps);
    m_clipDuration->setText(durTc.toString()
                            + QStringLiteral(" (")
                            + durTc.toSpokenString()
                            + QStringLiteral(")"));

    m_announcer->announce(
        tr("In point set to %1").arg(newIn.toString()),
        Announcer::Priority::Normal);
}

void PropertiesPanel::onOutPointEdited()
{
    if (!m_currentClip)
        return;

    const double fps = m_currentClip->outPoint().fps();
    const TimeCode newOut = TimeCode::fromString(m_clipOutPoint->text(), fps);

    if (newOut == m_currentClip->outPoint())
        return; // unchanged

    if (newOut <= m_currentClip->inPoint()) {
        // Revert to current value
        m_clipOutPoint->setText(m_currentClip->outPoint().toString());
        m_announcer->announce(
            tr("Invalid out point. Must be after the in point."),
            Announcer::Priority::Normal);
        return;
    }

    m_undoStack->push(new TrimClipCommand(
        m_currentClip, TrimClipCommand::Edge::Out, newOut));
    emit clipTrimmed();

    // Refresh duration display
    const int64_t dur = newOut.frame() - m_currentClip->inPoint().frame();
    TimeCode durTc(dur, fps);
    m_clipDuration->setText(durTc.toString()
                            + QStringLiteral(" (")
                            + durTc.toSpokenString()
                            + QStringLiteral(")"));

    m_announcer->announce(
        tr("Out point set to %1").arg(newOut.toString()),
        Announcer::Priority::Normal);
}

// ── effects ──────────────────────────────────────────────────────────

void PropertiesPanel::clearEffects()
{
    for (auto *w : m_effectWidgets)
        delete w;
    m_effectWidgets.clear();
    m_effectsGroup->setVisible(false);
}

void PropertiesPanel::populateEffects(const QVector<Effect *> &effects)
{
    clearEffects();

    if (effects.isEmpty())
        return;

    m_effectsGroup->setVisible(true);

    for (Effect *effect : effects) {
        // Per-effect container
        auto *card = new QGroupBox(effect->displayName(), m_effectsGroup);
        card->setAccessibleName(
            tr("Effect: %1").arg(effect->displayName()));
        auto *cardLayout = new QFormLayout(card);

        // Enabled toggle
        auto *enabledCb = new QCheckBox(tr("Enabled"), card);
        enabledCb->setChecked(effect->isEnabled());
        enabledCb->setAccessibleName(
            tr("%1 enabled").arg(effect->displayName()));
        cardLayout->addRow(enabledCb);
        connect(enabledCb, &QCheckBox::toggled,
                this, [this, effect](bool checked) {
                    if (checked != effect->isEnabled())
                        m_undoStack->push(
                            new SetEffectEnabledCommand(effect, checked));
                });

        // Parameter widgets
        for (const auto &param : effect->parameters()) {
            const QString &type = param.type;

            if (type == QStringLiteral("float")
                || type == QStringLiteral("double")) {
                auto *spin = new QDoubleSpinBox(card);
                spin->setAccessibleName(param.displayName);
                if (param.minimum.isValid())
                    spin->setMinimum(param.minimum.toDouble());
                if (param.maximum.isValid())
                    spin->setMaximum(param.maximum.toDouble());
                else
                    spin->setMaximum(99999.0);
                spin->setSingleStep(0.01);
                spin->setValue(param.currentValue.toDouble());

                const QString paramId = param.id;
                connect(spin, &QDoubleSpinBox::valueChanged,
                        this, [this, effect, paramId](double v) {
                            m_undoStack->push(
                                new ChangeEffectParameterCommand(
                                    effect, paramId, v));
                        });
                cardLayout->addRow(param.displayName + QStringLiteral(":"),
                                   spin);

            } else if (type == QStringLiteral("int")) {
                auto *spin = new QSpinBox(card);
                spin->setAccessibleName(param.displayName);
                if (param.minimum.isValid())
                    spin->setMinimum(param.minimum.toInt());
                if (param.maximum.isValid())
                    spin->setMaximum(param.maximum.toInt());
                else
                    spin->setMaximum(999999);
                spin->setValue(param.currentValue.toInt());

                const QString paramId = param.id;
                connect(spin, &QSpinBox::valueChanged,
                        this, [this, effect, paramId](int v) {
                            m_undoStack->push(
                                new ChangeEffectParameterCommand(
                                    effect, paramId, v));
                        });
                cardLayout->addRow(param.displayName + QStringLiteral(":"),
                                   spin);

            } else if (type == QStringLiteral("bool")) {
                auto *cb = new QCheckBox(param.displayName, card);
                cb->setAccessibleName(param.displayName);
                cb->setChecked(param.currentValue.toBool());

                const QString paramId = param.id;
                connect(cb, &QCheckBox::toggled,
                        this, [this, effect, paramId](bool v) {
                            m_undoStack->push(
                                new ChangeEffectParameterCommand(
                                    effect, paramId, v));
                        });
                cardLayout->addRow(cb);

            } else {
                // string, color, or unknown → plain QLineEdit
                auto *edit = new QLineEdit(
                    param.currentValue.toString(), card);
                edit->setAccessibleName(param.displayName);

                const QString paramId = param.id;
                connect(edit, &QLineEdit::editingFinished,
                        this, [this, effect, paramId, edit]() {
                            m_undoStack->push(
                                new ChangeEffectParameterCommand(
                                    effect, paramId, edit->text()));
                        });
                cardLayout->addRow(param.displayName + QStringLiteral(":"),
                                   edit);
            }
        }

        m_effectsLayout->addWidget(card);
        m_effectWidgets.append(card);
    }
}

} // namespace Thrive
