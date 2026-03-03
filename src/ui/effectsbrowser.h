// SPDX-License-Identifier: MIT
// Thrive Video Suite – Effects browser (search, preview, apply)

#pragma once

#include <QWidget>

QT_FORWARD_DECLARE_CLASS(QLineEdit)
QT_FORWARD_DECLARE_CLASS(QListWidget)
QT_FORWARD_DECLARE_CLASS(QPushButton)
QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QVBoxLayout)
QT_FORWARD_DECLARE_CLASS(QListWidgetItem)

namespace Thrive {

class EffectCatalog;
class Announcer;

/// Lets the user browse, search, and apply effects / transitions
/// from the EffectCatalog.  Every item has an accessible name and a
/// spoken description.
class EffectsBrowser : public QWidget
{
    Q_OBJECT

public:
    explicit EffectsBrowser(EffectCatalog *catalog,
                            Announcer *announcer,
                            QWidget *parent = nullptr);

signals:
    /// User chose to apply the effect identified by \a serviceId.
    void effectChosen(const QString &serviceId);

private slots:
    void onSearchTextChanged(const QString &text);
    void onItemActivated(QListWidgetItem *item);
    void onCurrentChanged(QListWidgetItem *current,
                          QListWidgetItem *previous);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void populateList(const QString &filter = {});

    EffectCatalog *m_catalog   = nullptr;
    Announcer     *m_announcer = nullptr;

    QLineEdit   *m_searchField = nullptr;
    QListWidget *m_list        = nullptr;
    QLabel      *m_description = nullptr;
    QVBoxLayout *m_layout      = nullptr;
};

} // namespace Thrive
