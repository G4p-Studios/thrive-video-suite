// SPDX-License-Identifier: MIT
// Thrive Video Suite - Stack manager dialog

#pragma once

#include <QDialog>

#include "../core/stackregistry.h"

QT_FORWARD_DECLARE_CLASS(QListWidget)
QT_FORWARD_DECLARE_CLASS(QPushButton)

namespace Thrive {

class Announcer;

class StackManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StackManagerDialog(Announcer *announcer,
                                QWidget *parent = nullptr);

signals:
    void stacksChanged();

private slots:
    void onCreate();
    void onImport();
    void onExport();
    void onRemove();
    void onSelectionChanged();

private:
    void reloadList();
    QString selectedStackId() const;

    Announcer *m_announcer = nullptr;
    StackRegistry m_registry;

    QListWidget *m_list = nullptr;
    QPushButton *m_btnCreate = nullptr;
    QPushButton *m_btnImport = nullptr;
    QPushButton *m_btnExport = nullptr;
    QPushButton *m_btnRemove = nullptr;
};

} // namespace Thrive
