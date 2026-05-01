// SPDX-License-Identifier: MIT
// Thrive Video Suite - Stack manager dialog implementation

#include "stackmanagerdialog.h"

#include "../accessibility/announcer.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace Thrive {

StackManagerDialog::StackManagerDialog(Announcer *announcer,
                                       QWidget *parent)
    : QDialog(parent)
    , m_announcer(announcer)
{
    setWindowTitle(tr("Stack Manager"));
    setMinimumWidth(540);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("Manage reusable stack templates. Built-in stacks are read-only."),
        this));

    m_list = new QListWidget(this);
    m_list->setAccessibleName(tr("Available stacks"));
    layout->addWidget(m_list, 1);

    auto *btnRow = new QHBoxLayout;
    m_btnCreate = new QPushButton(tr("Create"), this);
    m_btnImport = new QPushButton(tr("Import..."), this);
    m_btnExport = new QPushButton(tr("Export..."), this);
    m_btnRemove = new QPushButton(tr("Remove"), this);
    btnRow->addWidget(m_btnCreate);
    btnRow->addWidget(m_btnImport);
    btnRow->addWidget(m_btnExport);
    btnRow->addWidget(m_btnRemove);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addWidget(box);

    connect(m_btnCreate, &QPushButton::clicked, this, &StackManagerDialog::onCreate);
    connect(m_btnImport, &QPushButton::clicked, this, &StackManagerDialog::onImport);
    connect(m_btnExport, &QPushButton::clicked, this, &StackManagerDialog::onExport);
    connect(m_btnRemove, &QPushButton::clicked, this, &StackManagerDialog::onRemove);
    connect(m_list, &QListWidget::currentRowChanged,
            this, [this](int) { onSelectionChanged(); });

    reloadList();
}

void StackManagerDialog::onCreate()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("Create Stack"), tr("Stack name:"),
        QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    StackTemplate stack;
    stack.id = name;
    stack.name = name.trimmed();
    stack.description = tr("Custom stack template.");

    stack.captionDefaultText = QInputDialog::getText(
        this, tr("Default Caption"), tr("Caption default text:"),
        QLineEdit::Normal, QString(), &ok);
    if (!ok)
        return;

    const auto includeSecond = QMessageBox::question(
        this,
        tr("Second Text Phase"),
        tr("Include a second text phase by default?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    stack.includeSecondaryByDefault = includeSecond == QMessageBox::Yes;

    stack.secondaryPhaseName = QInputDialog::getText(
        this, tr("Second Phase Name"), tr("Second phase label:"),
        QLineEdit::Normal, tr("Second text"), &ok);
    if (!ok)
        return;

    stack.secondaryDefaultText = QInputDialog::getText(
        this, tr("Second Phase Default Text"), tr("Second phase default text:"),
        QLineEdit::Normal, QString(), &ok);
    if (!ok)
        return;

    stack.totalSeconds = QInputDialog::getDouble(
        this, tr("Total Duration"), tr("Default total duration (seconds):"),
        6.0, 1.0, 120.0, 1, &ok);
    if (!ok)
        return;

    stack.overlayStartSeconds = QInputDialog::getDouble(
        this, tr("Overlay Start"), tr("Default overlay start (seconds):"),
        0.6, 0.0, stack.totalSeconds, 1, &ok);
    if (!ok)
        return;

    stack.captionStartSeconds = QInputDialog::getDouble(
        this, tr("Caption Start"), tr("Default caption start (seconds):"),
        1.2, 0.0, stack.totalSeconds, 1, &ok);
    if (!ok)
        return;

    stack.secondaryStartSeconds = QInputDialog::getDouble(
        this, tr("Second Phase Start"), tr("Default second phase start (seconds):"),
        3.5, 0.0, stack.totalSeconds, 1, &ok);
    if (!ok)
        return;

    stack.fadeSeconds = QInputDialog::getDouble(
        this, tr("Fade Duration"), tr("Default fade duration (seconds):"),
        0.8, 0.1, 30.0, 1, &ok);
    if (!ok)
        return;

    const auto includeAudio = QMessageBox::question(
        this,
        tr("Default Soundtrack"),
        tr("Include soundtrack by default when this stack is added?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    stack.includeAudioByDefault = includeAudio == QMessageBox::Yes;

    stack.audioStartSeconds = QInputDialog::getDouble(
        this, tr("Soundtrack Start"), tr("Default soundtrack start (seconds):"),
        0.0, 0.0, stack.totalSeconds, 1, &ok);
    if (!ok)
        return;

    QString error;
    if (!m_registry.saveCustomStack(stack, &error)) {
        QMessageBox::warning(this, tr("Create Stack"), error);
        return;
    }

    reloadList();
    emit stacksChanged();
    if (m_announcer)
        m_announcer->announce(tr("Stack created."), Announcer::Priority::Normal);
}

void StackManagerDialog::onImport()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Import Stack"),
        QString(),
        StackRegistry::stackFileFilter());
    if (path.isEmpty())
        return;

    QString error;
    if (!m_registry.importStackFile(path, nullptr, &error)) {
        QMessageBox::warning(this, tr("Import Stack"), error);
        return;
    }

    reloadList();
    emit stacksChanged();
    if (m_announcer)
        m_announcer->announce(tr("Stack imported."), Announcer::Priority::Normal);
}

void StackManagerDialog::onExport()
{
    const QString id = selectedStackId();
    if (id.isEmpty())
        return;

    const QString path = QFileDialog::getSaveFileName(
        this,
        tr("Export Stack"),
        QString(),
        StackRegistry::stackFileFilter());
    if (path.isEmpty())
        return;

    QString outPath = path;
    if (!outPath.endsWith(QStringLiteral(".tstk"), Qt::CaseInsensitive))
        outPath += QStringLiteral(".tstk");

    QString error;
    if (!m_registry.exportStackFile(id, outPath, &error)) {
        QMessageBox::warning(this, tr("Export Stack"), error);
        return;
    }

    if (m_announcer)
        m_announcer->announce(tr("Stack exported."), Announcer::Priority::Normal);
}

void StackManagerDialog::onRemove()
{
    const QString id = selectedStackId();
    if (id.isEmpty())
        return;

    if (id.startsWith(QStringLiteral("builtin."))) {
        QMessageBox::information(this, tr("Remove Stack"),
                                 tr("Built-in stacks cannot be removed."));
        return;
    }

    if (QMessageBox::question(this, tr("Remove Stack"),
                              tr("Remove selected stack?"),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    QString error;
    if (!m_registry.deleteCustomStack(id, &error)) {
        QMessageBox::warning(this, tr("Remove Stack"), error);
        return;
    }

    reloadList();
    emit stacksChanged();
    if (m_announcer)
        m_announcer->announce(tr("Stack removed."), Announcer::Priority::Normal);
}

void StackManagerDialog::onSelectionChanged()
{
    const QString id = selectedStackId();
    const bool hasSelection = !id.isEmpty();
    m_btnExport->setEnabled(hasSelection);
    m_btnRemove->setEnabled(hasSelection && !id.startsWith(QStringLiteral("builtin.")));
}

void StackManagerDialog::reloadList()
{
    m_registry.reload();
    m_list->clear();

    const auto stacks = m_registry.allStacks();
    for (const auto &stack : stacks) {
        QString text = stack.name;
        if (stack.id.startsWith(QStringLiteral("builtin.")))
            text += tr(" (Built-in)");
        auto *item = new QListWidgetItem(text, m_list);
        item->setData(Qt::UserRole, stack.id);
        item->setToolTip(stack.description);
    }

    if (m_list->count() > 0)
        m_list->setCurrentRow(0);

    onSelectionChanged();
}

QString StackManagerDialog::selectedStackId() const
{
    auto *item = m_list->currentItem();
    if (!item)
        return QString();
    return item->data(Qt::UserRole).toString();
}

} // namespace Thrive
