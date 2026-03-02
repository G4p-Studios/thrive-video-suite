// SPDX-License-Identifier: MIT
// Thrive Video Suite – Keyboard shortcut editor

#pragma once

#include <QWidget>
#include <QHash>
#include <QKeySequence>

QT_FORWARD_DECLARE_CLASS(QTreeWidget)
QT_FORWARD_DECLARE_CLASS(QTreeWidgetItem)
QT_FORWARD_DECLARE_CLASS(QKeySequenceEdit)
QT_FORWARD_DECLARE_CLASS(QPushButton)
QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QVBoxLayout)

namespace Thrive {

class Announcer;

/// Lets the user view and edit all keyboard shortcuts.  Warns when a
/// chosen key sequence conflicts with a screen reader modifier
/// (Insert, CapsLock, ScrollLock) or with another action.
class ShortcutEditor : public QWidget
{
    Q_OBJECT

public:
    explicit ShortcutEditor(Announcer *announcer,
                            QWidget *parent = nullptr);

    /// Load shortcuts from a map of actionId → QKeySequence.
    void loadShortcuts(const QHash<QString, QKeySequence> &map);

    /// Return the current (possibly edited) shortcut map.
    [[nodiscard]] QHash<QString, QKeySequence> shortcuts() const;

signals:
    /// Emitted whenever the user changes a shortcut.
    void shortcutsChanged();

private slots:
    void onItemSelected();
    void onKeySequenceChanged(const QKeySequence &seq);
    void onResetClicked();

private:
    bool hasScreenReaderConflict(const QKeySequence &seq) const;
    bool hasDuplicateConflict(const QString &actionId,
                              const QKeySequence &seq) const;

    Announcer       *m_announcer      = nullptr;
    QTreeWidget     *m_tree           = nullptr;
    QKeySequenceEdit *m_seqEdit       = nullptr;
    QPushButton     *m_btnReset       = nullptr;
    QLabel          *m_warningLabel   = nullptr;
    QVBoxLayout     *m_layout         = nullptr;

    QString m_currentActionId;
};

} // namespace Thrive
