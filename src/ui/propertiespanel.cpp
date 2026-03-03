// SPDX-License-Identifier: MIT
// Thrive Video Suite – Properties panel implementation

#include "propertiespanel.h"
#include "../core/clip.h"
#include "../core/track.h"
#include "../core/effect.h"
#include "../core/transition.h"
#include "../core/timecode.h"
#include "../core/commands.h"
#include "../accessibility/announcer.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTextEdit>
#include <QTimer>
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

    // ── Transitions section ──────────────────────────────────────────
    buildTransitionsSection();
    mainLayout->addWidget(m_transitionsGroup);

    mainLayout->addStretch();

    m_scrollArea->setWidget(m_formWidget);

    // Description debounce timer (500ms)
    m_descDebounce = new QTimer(this);
    m_descDebounce->setSingleShot(true);
    m_descDebounce->setInterval(500);
    connect(m_descDebounce, &QTimer::timeout,
            this, &PropertiesPanel::commitDescription);

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

void PropertiesPanel::buildTransitionsSection()
{
    m_transitionsGroup = new QGroupBox(tr("Transitions"), m_formWidget);
    m_transitionsGroup->setAccessibleName(tr("Clip transitions"));
    m_transitionsLayout = new QVBoxLayout(m_transitionsGroup);
    m_transitionsGroup->setVisible(false);
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

    // Populate transitions
    populateTransitions(clip);

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

    // Show track-level effects (read-only – no reordering/removal)
    populateEffects(track->trackEffects(), /*readOnly=*/true);
    clearTransitions();

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
    clearTransitions();
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
        const QString newName = m_clipName->text();
        if (newName != m_currentTrack->name())
            m_undoStack->push(
                new RenameTrackCommand(m_currentTrack, newName));
    }
}

void PropertiesPanel::onClipDescriptionEdited()
{
    if (!m_currentClip) return;

    m_pendingDesc = m_clipDescription->toPlainText();
    m_descDebounce->start(); // restart the 500ms timer
}

void PropertiesPanel::commitDescription()
{
    if (!m_currentClip) return;

    if (m_pendingDesc != m_currentClip->description())
        m_undoStack->push(
            new ChangeClipDescriptionCommand(m_currentClip, m_pendingDesc));
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

void PropertiesPanel::populateEffects(const QVector<Effect *> &effects,
                                      bool readOnly)
{
    clearEffects();

    if (effects.isEmpty())
        return;

    m_effectsGroup->setVisible(true);

    for (int i = 0; i < effects.size(); ++i) {
        Effect *effect = effects.at(i);

        // Per-effect container
        auto *card = new QGroupBox(effect->displayName(), m_effectsGroup);
        card->setAccessibleName(
            tr("Effect: %1").arg(effect->displayName()));
        auto *cardLayout = new QFormLayout(card);

        // ── Action buttons (only for clip effects, not track effects) ──
        if (!readOnly) {
            auto *btnRow = new QHBoxLayout;

            auto *btnRemove = new QPushButton(tr("Remove"), card);
            btnRemove->setAccessibleName(
                tr("Remove effect %1").arg(effect->displayName()));
            connect(btnRemove, &QPushButton::clicked,
                    this, [this, i]() {
                        if (!m_currentClip) return;
                        m_undoStack->push(
                            new RemoveEffectCommand(m_currentClip, i));
                        populateEffects(m_currentClip->effects());
                        emit effectChanged();
                    });
            btnRow->addWidget(btnRemove);

            auto *btnUp = new QPushButton(tr("Up"), card);
            btnUp->setAccessibleName(
                tr("Move %1 up").arg(effect->displayName()));
            btnUp->setEnabled(i > 0);
            connect(btnUp, &QPushButton::clicked,
                    this, [this, i]() {
                        if (!m_currentClip || i <= 0) return;
                        m_undoStack->push(
                            new MoveEffectCommand(m_currentClip, i, i - 1));
                        populateEffects(m_currentClip->effects());
                        emit effectChanged();
                    });
            btnRow->addWidget(btnUp);

            auto *btnDown = new QPushButton(tr("Down"), card);
            btnDown->setAccessibleName(
                tr("Move %1 down").arg(effect->displayName()));
            btnDown->setEnabled(i < effects.size() - 1);
            connect(btnDown, &QPushButton::clicked,
                    this, [this, i]() {
                        if (!m_currentClip) return;
                        if (i >= m_currentClip->effects().size() - 1) return;
                        m_undoStack->push(
                            new MoveEffectCommand(m_currentClip, i, i + 1));
                        populateEffects(m_currentClip->effects());
                        emit effectChanged();
                    });
            btnRow->addWidget(btnDown);
            btnRow->addStretch();
            cardLayout->addRow(btnRow);
        }

        // Enabled toggle
        auto *enabledCb = new QCheckBox(tr("Enabled"), card);
        enabledCb->setChecked(effect->isEnabled());
        enabledCb->setAccessibleName(
            tr("%1 enabled").arg(effect->displayName()));
        cardLayout->addRow(enabledCb);
        connect(enabledCb, &QCheckBox::toggled,
                this, [this, effect](bool checked) {
                    if (checked != effect->isEnabled()) {
                        m_undoStack->push(
                            new SetEffectEnabledCommand(effect, checked));
                        emit effectChanged();
                    }
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
                            emit effectChanged();
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
                            emit effectChanged();
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
                            emit effectChanged();
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
                            emit effectChanged();
                        });
                cardLayout->addRow(param.displayName + QStringLiteral(":"),
                                   edit);
            }
        }

        m_effectsLayout->addWidget(card);
        m_effectWidgets.append(card);
    }
}

// ── transitions ──────────────────────────────────────────────────────

void PropertiesPanel::clearTransitions()
{
    for (auto *w : m_transitionWidgets)
        delete w;
    m_transitionWidgets.clear();
    m_transitionsGroup->setVisible(false);
}

void PropertiesPanel::populateTransitions(Clip *clip)
{
    clearTransitions();

    if (!clip) return;
    if (!clip->inTransition() && !clip->outTransition()) return;

    m_transitionsGroup->setVisible(true);

    auto addTransitionCard = [&](Transition *trans, bool isIn) {
        if (!trans) return;

        const QString edgeLabel = isIn ? tr("In Transition")
                                       : tr("Out Transition");
        auto *card = new QGroupBox(
            QStringLiteral("%1: %2").arg(edgeLabel, trans->displayName()),
            m_transitionsGroup);
        card->setAccessibleName(
            tr("%1 %2").arg(edgeLabel, trans->displayName()));
        auto *layout = new QFormLayout(card);

        // Duration editor
        auto *durSpin = new QDoubleSpinBox(card);
        durSpin->setAccessibleName(tr("Transition duration seconds"));
        durSpin->setMinimum(0.1);
        durSpin->setMaximum(30.0);
        durSpin->setSingleStep(0.1);
        durSpin->setDecimals(1);
        const double fps = clip->inPoint().fps() > 0 ? clip->inPoint().fps() : 25.0;
        durSpin->setValue(trans->duration().frame() / fps);
        layout->addRow(tr("Duration (s):"), durSpin);

        connect(durSpin, &QDoubleSpinBox::valueChanged,
                this, [this, trans, fps](double val) {
                    int frames = static_cast<int>(val * fps);
                    m_undoStack->push(
                        new ChangeTransitionDurationCommand(
                            trans, TimeCode(frames, fps)));
                    emit effectChanged();
                });

        // Remove button
        auto *btnRemove = new QPushButton(tr("Remove"), card);
        btnRemove->setAccessibleName(
            tr("Remove %1").arg(edgeLabel));
        using Edge = AddTransitionCommand::Edge;
        Edge edge = isIn ? Edge::In : Edge::Out;
        connect(btnRemove, &QPushButton::clicked,
                this, [this, clip, edge]() {
                    m_undoStack->push(
                        new RemoveTransitionCommand(clip, edge));
                    populateTransitions(clip);
                    emit effectChanged();
                });
        layout->addRow(btnRemove);

        m_transitionsLayout->addWidget(card);
        m_transitionWidgets.append(card);
    };

    addTransitionCard(clip->inTransition(), true);
    addTransitionCard(clip->outTransition(), false);
}

} // namespace Thrive
