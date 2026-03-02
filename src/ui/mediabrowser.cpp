// SPDX-License-Identifier: MIT
// Thrive Video Suite – Media browser implementation

#include "mediabrowser.h"
#include "../accessibility/announcer.h"

#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFileInfo>

namespace Thrive {

MediaBrowser::MediaBrowser(Announcer *announcer, QWidget *parent)
    : QWidget(parent)
    , m_announcer(announcer)
    , m_layout(new QVBoxLayout(this))
{
    setObjectName(QStringLiteral("MediaBrowser"));
    setAccessibleName(tr("Media browser"));

    m_list = new QListWidget(this);
    m_list->setAccessibleName(tr("Imported media files"));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);

    auto *btnLayout = new QHBoxLayout;
    m_btnImport = new QPushButton(tr("&Import…"), this);
    m_btnImport->setAccessibleName(tr("Import media files"));
    m_btnRemove = new QPushButton(tr("&Remove"), this);
    m_btnRemove->setAccessibleName(tr("Remove selected file"));

    btnLayout->addWidget(m_btnImport);
    btnLayout->addWidget(m_btnRemove);
    btnLayout->addStretch();

    m_layout->addWidget(m_list);
    m_layout->addLayout(btnLayout);

    connect(m_btnImport, &QPushButton::clicked,
            this, &MediaBrowser::importFiles);
    connect(m_btnRemove, &QPushButton::clicked,
            this, &MediaBrowser::removeSelected);
    connect(m_list, &QListWidget::itemActivated,
            this, &MediaBrowser::onItemActivated);
    connect(m_list, &QListWidget::currentItemChanged,
            this, &MediaBrowser::onCurrentChanged);
}

QStringList MediaBrowser::files() const
{
    QStringList result;
    for (int i = 0; i < m_list->count(); ++i) {
        result << m_list->item(i)->data(Qt::UserRole).toString();
    }
    return result;
}

void MediaBrowser::importFiles()
{
    const QStringList paths = QFileDialog::getOpenFileNames(
        this,
        tr("Import Media"),
        QString(),
        tr("Media Files (*.mp4 *.mkv *.mov *.avi *.webm *.mp3 *.wav "
           "*.flac *.ogg *.png *.jpg *.jpeg *.bmp *.tiff);;All Files (*)"));

    if (paths.isEmpty()) return;

    for (const QString &path : paths) {
        QFileInfo fi(path);
        auto *item = new QListWidgetItem(fi.fileName(), m_list);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
    }

    const int count = paths.size();
    m_announcer->announce(
        tr("%n file(s) imported.", nullptr, count),
        Announcer::Priority::Normal);
}

void MediaBrowser::removeSelected()
{
    auto *item = m_list->currentItem();
    if (!item) {
        m_announcer->announce(tr("No file selected."),
                              Announcer::Priority::Normal);
        return;
    }
    const QString name = item->text();
    delete m_list->takeItem(m_list->row(item));
    m_announcer->announce(
        tr("%1 removed.").arg(name), Announcer::Priority::Normal);
}

void MediaBrowser::onItemActivated(QListWidgetItem *item)
{
    if (item) {
        emit fileActivated(item->data(Qt::UserRole).toString());
    }
}

void MediaBrowser::onCurrentChanged(QListWidgetItem *current,
                                    QListWidgetItem * /*previous*/)
{
    if (!current) return;
    m_announcer->announce(current->text(), Announcer::Priority::Low);
}

} // namespace Thrive
