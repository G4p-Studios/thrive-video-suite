// SPDX-License-Identifier: MIT
// Thrive Video Suite – Shortcut manager implementation

#include "shortcutmanager.h"
#include "constants.h"

#include <QAction>
#include <QSettings>

namespace Thrive {

ShortcutManager &ShortcutManager::instance()
{
    static ShortcutManager s;
    return s;
}

ShortcutManager::ShortcutManager()
    : QObject(nullptr)
{
}

void ShortcutManager::registerAction(const QString &id,
                                     QAction *action,
                                     const QKeySequence &defaultShortcut)
{
    Entry e;
    e.action          = action;
    e.defaultShortcut = defaultShortcut;
    e.currentShortcut = defaultShortcut;
    action->setShortcut(defaultShortcut);
    m_entries.insert(id, e);
}

QKeySequence ShortcutManager::shortcutFor(const QString &id) const
{
    auto it = m_entries.constFind(id);
    if (it != m_entries.cend()) {
        return it->currentShortcut;
    }
    return {};
}

void ShortcutManager::setShortcut(const QString &id,
                                  const QKeySequence &seq)
{
    auto it = m_entries.find(id);
    if (it == m_entries.end()) return;

    it->currentShortcut = seq;
    if (it->action) {
        it->action->setShortcut(seq);
    }
    save();
    emit shortcutsChanged();
}

void ShortcutManager::resetAll()
{
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
        it->currentShortcut = it->defaultShortcut;
        if (it->action) {
            it->action->setShortcut(it->defaultShortcut);
        }
    }
    save();
    emit shortcutsChanged();
}

void ShortcutManager::load()
{
    QSettings settings;
    settings.beginGroup(QLatin1String(kSettingsShortcuts));
    const QStringList keys = settings.childKeys();
    for (const QString &key : keys) {
        auto it = m_entries.find(key);
        if (it == m_entries.end()) continue;
        const QKeySequence seq(
            settings.value(key).toString(), QKeySequence::PortableText);
        it->currentShortcut = seq;
        if (it->action) {
            it->action->setShortcut(seq);
        }
    }
    settings.endGroup();
}

void ShortcutManager::save() const
{
    QSettings settings;
    settings.beginGroup(QLatin1String(kSettingsShortcuts));
    for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it) {
        if (it->currentShortcut != it->defaultShortcut) {
            settings.setValue(it.key(),
                              it->currentShortcut.toString(
                                  QKeySequence::PortableText));
        } else {
            settings.remove(it.key());
        }
    }
    settings.endGroup();
}

QHash<QString, QKeySequence> ShortcutManager::toMap() const
{
    QHash<QString, QKeySequence> map;
    for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it) {
        map.insert(it.key(), it->currentShortcut);
    }
    return map;
}

void ShortcutManager::applyMap(const QHash<QString, QKeySequence> &map)
{
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        setShortcut(it.key(), it.value());
    }
}

} // namespace Thrive
