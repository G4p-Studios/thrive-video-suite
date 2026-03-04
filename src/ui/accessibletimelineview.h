// SPDX-License-Identifier: MIT
// Thrive Video Suite – QAccessibleInterface for timeline cells

#pragma once

#include <QAccessibleWidget>
#include <QAccessibleTableInterface>
#include <QAccessibleTableCellInterface>

namespace Thrive {

class TimelineWidget;
class Timeline;

/// Call once at startup (from main.cpp) to ensure the accessible
/// factory is linked and registered with QAccessible.
void registerAccessibleTimelineFactory();

/// Exposes the TimelineWidget as an accessible table so that screen
/// readers can navigate rows (tracks) and columns (clips) with their
/// native table commands (e.g. Ctrl+Alt+Arrows in NVDA).
class AccessibleTimelineView : public QAccessibleWidget,
                               public QAccessibleTableInterface
{
public:
    explicit AccessibleTimelineView(TimelineWidget *widget);

    // ── QAccessibleInterface ────────────────────────────────────────
    void       *interface_cast(QAccessible::InterfaceType type) override;
    QAccessible::Role role() const override;
    QString     text(QAccessible::Text t) const override;
    int         childCount() const override;
    QAccessibleInterface *child(int index) const override;
    int         indexOfChild(const QAccessibleInterface *child) const override;
    QAccessibleInterface *focusChild() const override;

    // ── QAccessibleTableInterface ───────────────────────────────────
    int  rowCount()    const override;
    int  columnCount() const override;

    QAccessibleInterface *cellAt(int row, int column) const override;

    QString rowDescription(int row)       const override;
    QString columnDescription(int column) const override;

    int  selectedCellCount()   const override;
    int  selectedColumnCount() const override;
    int  selectedRowCount()    const override;
    QList<QAccessibleInterface *> selectedCells()   const override;
    QList<int>                    selectedColumns()  const override;
    QList<int>                    selectedRows()     const override;
    bool isColumnSelected(int column) const override;
    bool isRowSelected(int row)       const override;

    bool selectRow(int row)       override;
    bool selectColumn(int column) override;
    bool unselectRow(int row)     override;
    bool unselectColumn(int column) override;

    QAccessibleInterface *caption()  const override;
    QAccessibleInterface *summary()  const override;

    void modelChange(QAccessibleTableModelChangeEvent *event) override;

private:
    Timeline *timeline() const;
};

// ─────────────────────────────────────────────────────────────────────
/// A single cell in the accessible table (one clip slot).
class AccessibleTimelineCell : public QAccessibleInterface,
                               public QAccessibleTableCellInterface
{
public:
    AccessibleTimelineCell(TimelineWidget *widget, int row, int col);

    // ── QAccessibleInterface ────────────────────────────────────────
    bool isValid() const override;

    QObject                *object()   const override;
    QAccessibleInterface   *parent()   const override;
    QAccessibleInterface   *child(int) const override { return nullptr; }
    QAccessibleInterface   *childAt(int, int) const override { return nullptr; }
    int                     childCount() const override { return 0; }
    int                     indexOfChild(const QAccessibleInterface *) const override { return -1; }

    QAccessible::Role  role()  const override;
    QAccessible::State state() const override;
    QRect              rect()  const override;

    QString text(QAccessible::Text t) const override;
    void    setText(QAccessible::Text, const QString &) override {}

    void *interface_cast(QAccessible::InterfaceType type) override;

    // ── QAccessibleTableCellInterface ───────────────────────────────
    int         rowIndex()    const override { return m_row; }
    int         columnIndex() const override { return m_col; }
    int         rowExtent()   const override { return 1; }
    int         columnExtent() const override { return 1; }
    bool        isSelected()  const override;
    QList<QAccessibleInterface *> rowHeaderCells()    const override;
    QList<QAccessibleInterface *> columnHeaderCells() const override;
    QAccessibleInterface         *table() const override;

private:
    TimelineWidget *m_widget;
    int             m_row;
    int             m_col;
};

} // namespace Thrive
