// SPDX-License-Identifier: MIT
// Thrive Video Suite – Properties panel (clip / effect / track inspector)

#pragma once

#include <QWidget>

QT_FORWARD_DECLARE_CLASS(QFormLayout)
QT_FORWARD_DECLARE_CLASS(QLineEdit)
QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QTextEdit)
QT_FORWARD_DECLARE_CLASS(QScrollArea)

namespace Thrive {

class Clip;
class Track;
class Announcer;

/// Displays and edits the properties of the currently focused clip or
/// track.  All form fields have accessible labels so that every
/// property name is announced before its value.
class PropertiesPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PropertiesPanel(Announcer *announcer,
                             QWidget *parent = nullptr);

    /// Show properties for the given clip (nullptr clears the panel).
    void inspectClip(Clip *clip);

    /// Show properties for the given track.
    void inspectTrack(Track *track);

    /// Clear the panel.
    void clear();

private slots:
    void onClipNameEdited();
    void onClipDescriptionEdited();

private:
    void buildClipForm();

    Announcer   *m_announcer    = nullptr;
    QScrollArea *m_scrollArea   = nullptr;
    QWidget     *m_formWidget   = nullptr;
    QFormLayout *m_formLayout   = nullptr;

    // Clip fields
    QLineEdit *m_clipName        = nullptr;
    QTextEdit *m_clipDescription = nullptr;
    QLabel    *m_clipSource      = nullptr;
    QLabel    *m_clipInPoint     = nullptr;
    QLabel    *m_clipOutPoint    = nullptr;
    QLabel    *m_clipDuration    = nullptr;

    Clip  *m_currentClip  = nullptr;
    Track *m_currentTrack = nullptr;
};

} // namespace Thrive
