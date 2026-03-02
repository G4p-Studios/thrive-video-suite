// SPDX-License-Identifier: MIT
// Thrive Video Suite – Effects browser implementation

#include "effectsbrowser.h"
#include "../engine/effectcatalog.h"
#include "../accessibility/announcer.h"

#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>

namespace Thrive {

EffectsBrowser::EffectsBrowser(EffectCatalog *catalog,
                               Announcer *announcer,
                               QWidget *parent)
    : QWidget(parent)
    , m_catalog(catalog)
    , m_announcer(announcer)
    , m_layout(new QVBoxLayout(this))
{
    setObjectName(QStringLiteral("EffectsBrowser"));
    setAccessibleName(tr("Effects browser"));

    // Search field
    m_searchField = new QLineEdit(this);
    m_searchField->setPlaceholderText(tr("Search effects…"));
    m_searchField->setAccessibleName(tr("Search effects"));
    m_searchField->setClearButtonEnabled(true);

    // Effect list
    m_list = new QListWidget(this);
    m_list->setAccessibleName(tr("Available effects"));

    // Description area
    m_description = new QLabel(this);
    m_description->setWordWrap(true);
    m_description->setAccessibleName(tr("Effect description"));

    m_layout->addWidget(m_searchField);
    m_layout->addWidget(m_list, 1);
    m_layout->addWidget(m_description);

    connect(m_searchField, &QLineEdit::textChanged,
            this, &EffectsBrowser::onSearchTextChanged);
    connect(m_list, &QListWidget::itemActivated,
            this, &EffectsBrowser::onItemActivated);
    connect(m_list, &QListWidget::currentItemChanged,
            this, &EffectsBrowser::onCurrentChanged);

    populateList();
}

void EffectsBrowser::populateList(const QString &filter)
{
    m_list->clear();

    const auto entries = filter.isEmpty()
                             ? m_catalog->allEntries()
                             : m_catalog->search(filter);

    for (const auto &entry : entries) {
        auto *item = new QListWidgetItem(entry.displayName, m_list);
        item->setData(Qt::UserRole, entry.serviceId);
        item->setData(Qt::UserRole + 1, entry.description);
        item->setToolTip(entry.description);
    }

    m_announcer->announce(
        tr("%n effect(s) shown.", nullptr, m_list->count()),
        Announcer::Priority::Low);
}

void EffectsBrowser::onSearchTextChanged(const QString &text)
{
    populateList(text);
}

void EffectsBrowser::onItemActivated(QListWidgetItem *item)
{
    if (!item) return;
    const QString serviceId = item->data(Qt::UserRole).toString();
    emit effectChosen(serviceId);
    m_announcer->announce(
        tr("Applied effect: %1").arg(item->text()),
        Announcer::Priority::Normal);
}

void EffectsBrowser::onCurrentChanged(QListWidgetItem *current,
                                      QListWidgetItem * /*previous*/)
{
    if (!current) {
        m_description->clear();
        return;
    }

    const QString desc = current->data(Qt::UserRole + 1).toString();
    m_description->setText(desc);
    m_announcer->announce(
        QStringLiteral("%1. %2").arg(current->text(), desc),
        Announcer::Priority::Low);
}

} // namespace Thrive
