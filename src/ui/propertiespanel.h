// SPDX-License-Identifier: MIT
// Thrive Video Suite – Properties panel (clip / effect / track inspector)

#pragma once

#include <QWidget>
#include <QVector>

QT_FORWARD_DECLARE_CLASS(QFormLayout)
QT_FORWARD_DECLARE_CLASS(QLineEdit)
QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QTextEdit)
QT_FORWARD_DECLARE_CLASS(QScrollArea)
QT_FORWARD_DECLARE_CLASS(QGroupBox)
QT_FORWARD_DECLARE_CLASS(QVBoxLayout)
QT_FORWARD_DECLARE_CLASS(QUndoStack)
QT_FORWARD_DECLARE_CLASS(QCheckBox)
QT_FORWARD_DECLARE_CLASS(QTimer)

namespace Thrive {

class Clip;
class Track;
class Effect;
class Transition;
class Announcer;

/// Displays and edits the properties of the currently focused clip or
/// track.  All form fields have accessible labels so that every
/// property name is announced before its value.
class PropertiesPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PropertiesPanel(Announcer *announcer,
                             QUndoStack *undoStack,
                             QWidget *parent = nullptr);

    /// Show properties for the given clip (nullptr clears the panel).
    void inspectClip(Clip *clip);

    /// Show properties for the given track.
    void inspectTrack(Track *track);

    /// Clear the panel.
    void clear();

signals:
    /// Emitted when an in/out point change requires a tractor rebuild.
    void clipTrimmed();

    /// Emitted when an effect is added, removed, reordered, or parameter
    /// changed – tells MainWindow to rebuild the tractor.
    void effectChanged();

private slots:
    void onClipNameEdited();
    void onClipDescriptionEdited();
    void commitDescription();
    void onInPointEdited();
    void onOutPointEdited();

private:
    void buildClipForm();
    void buildEffectsSection();
    void buildTransitionsSection();
    void populateEffects(const QVector<Effect *> &effects);
    void populateTransitions(Clip *clip);
    void clearEffects();
    void clearTransitions();

    Announcer  *m_announcer  = nullptr;
    QUndoStack *m_undoStack  = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QWidget     *m_formWidget = nullptr;
    QFormLayout *m_formLayout = nullptr;

    // Clip / track fields
    QLineEdit *m_clipName        = nullptr;
    QTextEdit *m_clipDescription = nullptr;
    QLabel    *m_clipSource      = nullptr;
    QLineEdit *m_clipInPoint     = nullptr;
    QLineEdit *m_clipOutPoint    = nullptr;
    QLabel    *m_clipDuration    = nullptr;

    // Effects section
    QGroupBox   *m_effectsGroup  = nullptr;
    QVBoxLayout *m_effectsLayout = nullptr;

    // Transitions section
    QGroupBox   *m_transitionsGroup  = nullptr;
    QVBoxLayout *m_transitionsLayout = nullptr;

    /// Widgets created dynamically for each effect – cleaned up on re-inspect
    QVector<QWidget *> m_effectWidgets;
    QVector<QWidget *> m_transitionWidgets;

    Clip  *m_currentClip  = nullptr;
    Track *m_currentTrack = nullptr;

    // Description debounce timer
    QTimer *m_descDebounce = nullptr;
    QString m_pendingDesc;
};

} // namespace Thrive
