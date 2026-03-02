// SPDX-License-Identifier: MIT
// Thrive Video Suite – Plugin manager widget

#pragma once

#include <QWidget>
#include <QVector>

QT_FORWARD_DECLARE_CLASS(QTreeWidget)
QT_FORWARD_DECLARE_CLASS(QPushButton)
QT_FORWARD_DECLARE_CLASS(QVBoxLayout)

namespace Thrive {

class Announcer;

/// Describes one installed plugin.
struct PluginInfo {
    QString id;          ///< Unique identifier (reverse-DNS)
    QString name;        ///< Human-readable display name
    QString version;     ///< Semver string
    QString description; ///< Spoken description
    QString path;        ///< Directory on disk
    bool    enabled = true;
};

/// Lists installed plugins and lets the user install / remove them.
/// Changes take effect after an app restart (EXIT_RESTART flow).
class PluginManager : public QWidget
{
    Q_OBJECT

public:
    explicit PluginManager(Announcer *announcer,
                           QWidget *parent = nullptr);

    /// Load the current plugin list into the tree.
    void setPlugins(const QVector<PluginInfo> &plugins);

    /// Return current list (reflects any install / remove actions).
    [[nodiscard]] QVector<PluginInfo> plugins() const;

signals:
    /// Emitted when the list changes (install / remove).
    void pluginsChanged();

    /// Emitted when the user confirms an action that requires restart.
    void restartRequired();

public slots:
    void installPlugin();
    void removeSelected();

private:
    void refreshTree();

    Announcer           *m_announcer = nullptr;
    QTreeWidget         *m_tree      = nullptr;
    QPushButton         *m_btnInstall = nullptr;
    QPushButton         *m_btnRemove  = nullptr;
    QVBoxLayout         *m_layout    = nullptr;
    QVector<PluginInfo>  m_plugins;
};

} // namespace Thrive
