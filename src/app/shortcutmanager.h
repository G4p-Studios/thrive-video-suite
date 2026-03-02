// SPDX-License-Identifier: MIT
// Thrive Video Suite – Shortcut manager (singleton registry)

#pragma once

#include <QObject>
#include <QHash>
#include <QKeySequence>

QT_FORWARD_DECLARE_CLASS(QAction)

namespace Thrive {

class Announcer;

/// Central registry for keyboard shortcuts.  Persists customised
/// bindings in QSettings and detects screen-reader modifier conflicts.
class ShortcutManager : public QObject
{
    Q_OBJECT

public:
    static ShortcutManager &instance();

    /// Register an action with a default shortcut.
    void registerAction(const QString &id,
                        QAction *action,
                        const QKeySequence &defaultShortcut);

    /// Return the current binding for an action.
    [[nodiscard]] QKeySequence shortcutFor(const QString &id) const;

    /// Override the shortcut for an action (and persist to QSettings).
    void setShortcut(const QString &id, const QKeySequence &seq);

    /// Reset all shortcuts to their defaults.
    void resetAll();

    /// Load customised shortcuts from QSettings.
    void load();

    /// Save all current shortcuts to QSettings.
    void save() const;

    /// Build a map of id → QKeySequence for the ShortcutEditor widget.
    [[nodiscard]] QHash<QString, QKeySequence> toMap() const;

    /// Apply a map of id → QKeySequence from the ShortcutEditor widget.
    void applyMap(const QHash<QString, QKeySequence> &map);

signals:
    void shortcutsChanged();

private:
    ShortcutManager();
    Q_DISABLE_COPY_MOVE(ShortcutManager)

    struct Entry {
        QAction      *action          = nullptr;
        QKeySequence  defaultShortcut;
        QKeySequence  currentShortcut;
    };

    QHash<QString, Entry> m_entries;
};

} // namespace Thrive
