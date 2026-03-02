// SPDX-License-Identifier: MIT
// Thrive Video Suite – Plugin manager implementation

#include "pluginmanager.h"
#include "../accessibility/announcer.h"

#include <QTreeWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>

#include <algorithm>

namespace Thrive {

PluginManager::PluginManager(Announcer *announcer, QWidget *parent)
    : QWidget(parent)
    , m_announcer(announcer)
    , m_layout(new QVBoxLayout(this))
{
    setObjectName(QStringLiteral("PluginManager"));
    setAccessibleName(tr("Plugin manager"));

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({ tr("Name"), tr("Version"), tr("Status") });
    m_tree->setAccessibleName(tr("Installed plugins"));
    m_tree->header()->setStretchLastSection(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);

    auto *btnLayout = new QHBoxLayout;
    m_btnInstall = new QPushButton(tr("&Install…"), this);
    m_btnInstall->setAccessibleName(tr("Install a new plugin"));
    m_btnRemove = new QPushButton(tr("&Remove"), this);
    m_btnRemove->setAccessibleName(tr("Remove selected plugin"));

    btnLayout->addWidget(m_btnInstall);
    btnLayout->addWidget(m_btnRemove);
    btnLayout->addStretch();

    m_layout->addWidget(m_tree, 1);
    m_layout->addLayout(btnLayout);

    connect(m_btnInstall, &QPushButton::clicked,
            this, &PluginManager::installPlugin);
    connect(m_btnRemove, &QPushButton::clicked,
            this, &PluginManager::removeSelected);
}

void PluginManager::setPlugins(const QVector<PluginInfo> &plugins)
{
    m_plugins = plugins;
    refreshTree();
}

QVector<PluginInfo> PluginManager::plugins() const
{
    return m_plugins;
}

void PluginManager::installPlugin()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select Plugin Directory"));
    if (dir.isEmpty()) return;

    // In a real implementation we'd parse plugin.json from the dir.
    // For now, create a placeholder entry.
    PluginInfo info;
    info.id          = QStringLiteral("com.user.plugin.%1").arg(m_plugins.size());
    info.name        = dir.section(QLatin1Char('/'), -1);
    info.version     = QStringLiteral("1.0.0");
    info.description = tr("User-installed plugin from %1").arg(dir);
    info.path        = dir;
    info.enabled     = true;

    m_plugins.append(info);
    refreshTree();

    m_announcer->announce(
        tr("Plugin '%1' installed. Restart required to activate.")
            .arg(info.name),
        Announcer::Priority::High);

    emit pluginsChanged();
    emit restartRequired();
}

void PluginManager::removeSelected()
{
    auto *item = m_tree->currentItem();
    if (!item) {
        m_announcer->announce(tr("No plugin selected."),
                              Announcer::Priority::Normal);
        return;
    }

    const QString pluginId = item->data(0, Qt::UserRole).toString();
    const QString name     = item->text(0);

    auto answer = QMessageBox::question(
        this,
        tr("Remove Plugin"),
        tr("Remove plugin '%1'?  This will take effect after restart.")
            .arg(name));

    if (answer != QMessageBox::Yes) return;

    m_plugins.erase(
        std::remove_if(m_plugins.begin(), m_plugins.end(),
                       [&](const PluginInfo &p) { return p.id == pluginId; }),
        m_plugins.end());

    refreshTree();

    m_announcer->announce(
        tr("Plugin '%1' removed. Restart required.").arg(name),
        Announcer::Priority::High);

    emit pluginsChanged();
    emit restartRequired();
}

void PluginManager::refreshTree()
{
    m_tree->clear();
    for (const auto &p : m_plugins) {
        auto *item = new QTreeWidgetItem(m_tree);
        item->setText(0, p.name);
        item->setText(1, p.version);
        item->setText(2, p.enabled ? tr("Enabled") : tr("Disabled"));
        item->setData(0, Qt::UserRole, p.id);
        item->setToolTip(0, p.description);
    }
}

} // namespace Thrive
