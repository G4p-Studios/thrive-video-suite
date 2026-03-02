// SPDX-License-Identifier: MIT
// Thrive Video Suite – Keyboard shortcut editor implementation

#include "shortcuteditor.h"
#include "../accessibility/announcer.h"

#include <QTreeWidget>
#include <QKeySequenceEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>

namespace Thrive {

ShortcutEditor::ShortcutEditor(Announcer *announcer, QWidget *parent)
    : QWidget(parent)
    , m_announcer(announcer)
    , m_layout(new QVBoxLayout(this))
{
    setObjectName(QStringLiteral("ShortcutEditor"));
    setAccessibleName(tr("Keyboard shortcuts"));

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({ tr("Action"), tr("Shortcut") });
    m_tree->setAccessibleName(tr("Shortcut list"));
    m_tree->header()->setStretchLastSection(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);

    auto *editRow = new QHBoxLayout;
    m_seqEdit = new QKeySequenceEdit(this);
    m_seqEdit->setAccessibleName(tr("New shortcut"));
    m_seqEdit->setEnabled(false);

    m_btnReset = new QPushButton(tr("Reset to &default"), this);
    m_btnReset->setAccessibleName(tr("Reset selected shortcut to default"));
    m_btnReset->setEnabled(false);

    editRow->addWidget(m_seqEdit, 1);
    editRow->addWidget(m_btnReset);

    m_warningLabel = new QLabel(this);
    m_warningLabel->setWordWrap(true);
    m_warningLabel->setAccessibleName(tr("Shortcut warning"));
    m_warningLabel->setVisible(false);

    m_layout->addWidget(m_tree, 1);
    m_layout->addLayout(editRow);
    m_layout->addWidget(m_warningLabel);

    connect(m_tree, &QTreeWidget::currentItemChanged,
            this, [this]() { onItemSelected(); });
    connect(m_seqEdit, &QKeySequenceEdit::keySequenceChanged,
            this, &ShortcutEditor::onKeySequenceChanged);
    connect(m_btnReset, &QPushButton::clicked,
            this, &ShortcutEditor::onResetClicked);
}

void ShortcutEditor::loadShortcuts(
    const QHash<QString, QKeySequence> &map)
{
    m_tree->clear();
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        auto *item = new QTreeWidgetItem(m_tree);
        item->setText(0, it.key());
        item->setText(1, it.value().toString(QKeySequence::NativeText));
        item->setData(0, Qt::UserRole, it.key());           // actionId
        item->setData(1, Qt::UserRole, it.value().toString(
                                           QKeySequence::PortableText)); // default
    }
    m_tree->sortByColumn(0, Qt::AscendingOrder);
}

QHash<QString, QKeySequence> ShortcutEditor::shortcuts() const
{
    QHash<QString, QKeySequence> result;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        auto *item = m_tree->topLevelItem(i);
        const QString id  = item->data(0, Qt::UserRole).toString();
        const QString seq = item->text(1);
        result.insert(id, QKeySequence(seq, QKeySequence::NativeText));
    }
    return result;
}

// ── slots ───────────────────────────────────────────────────────────

void ShortcutEditor::onItemSelected()
{
    auto *item = m_tree->currentItem();
    if (!item) {
        m_seqEdit->setEnabled(false);
        m_btnReset->setEnabled(false);
        return;
    }

    m_currentActionId = item->data(0, Qt::UserRole).toString();
    m_seqEdit->setEnabled(true);
    m_btnReset->setEnabled(true);
    m_seqEdit->setKeySequence(
        QKeySequence(item->text(1), QKeySequence::NativeText));
    m_warningLabel->setVisible(false);

    m_announcer->announce(
        tr("Editing shortcut for %1, current: %2")
            .arg(m_currentActionId, item->text(1)),
        Announcer::Priority::Normal);
}

void ShortcutEditor::onKeySequenceChanged(const QKeySequence &seq)
{
    if (m_currentActionId.isEmpty()) return;

    // Screen reader conflict check
    if (hasScreenReaderConflict(seq)) {
        const QString warn = tr("Warning: this key combination uses a "
                                "screen reader modifier (Insert, Caps Lock, "
                                "or Scroll Lock) and may conflict with your "
                                "screen reader.");
        m_warningLabel->setText(warn);
        m_warningLabel->setVisible(true);
        m_announcer->announce(warn, Announcer::Priority::High);
    }
    // Duplicate check
    else if (hasDuplicateConflict(m_currentActionId, seq)) {
        const QString warn = tr("Warning: this shortcut is already "
                                "assigned to another action.");
        m_warningLabel->setText(warn);
        m_warningLabel->setVisible(true);
        m_announcer->announce(warn, Announcer::Priority::High);
    } else {
        m_warningLabel->setVisible(false);
    }

    // Apply to tree
    auto *item = m_tree->currentItem();
    if (item) {
        item->setText(1, seq.toString(QKeySequence::NativeText));
    }
    emit shortcutsChanged();
}

void ShortcutEditor::onResetClicked()
{
    auto *item = m_tree->currentItem();
    if (!item) return;

    const QString defaultSeq = item->data(1, Qt::UserRole).toString();
    m_seqEdit->setKeySequence(
        QKeySequence(defaultSeq, QKeySequence::PortableText));
    m_announcer->announce(tr("Reset to default: %1")
                              .arg(m_seqEdit->keySequence().toString(
                                  QKeySequence::NativeText)),
                          Announcer::Priority::Normal);
}

// ── conflict detection ──────────────────────────────────────────────

bool ShortcutEditor::hasScreenReaderConflict(
    const QKeySequence &seq) const
{
    const QString portable = seq.toString(QKeySequence::PortableText)
                                 .toLower();
    // Screen reader modifiers
    static const QStringList blocklist = {
        QStringLiteral("ins+"),
        QStringLiteral("insert+"),
        QStringLiteral("capslock+"),
        QStringLiteral("caps lock+"),
        QStringLiteral("scrolllock+"),
        QStringLiteral("scroll lock+"),
    };
    for (const QString &prefix : blocklist) {
        if (portable.contains(prefix)) {
            return true;
        }
    }
    return false;
}

bool ShortcutEditor::hasDuplicateConflict(const QString &actionId,
                                          const QKeySequence &seq) const
{
    if (seq.isEmpty()) return false;

    const QString seqText = seq.toString(QKeySequence::PortableText);
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        auto *item = m_tree->topLevelItem(i);
        if (item->data(0, Qt::UserRole).toString() == actionId) continue;
        if (item->text(1) == seq.toString(QKeySequence::NativeText)) {
            return true;
        }
    }
    return false;
}

} // namespace Thrive
