// SPDX-License-Identifier: MIT
// Thrive Video Suite – QAccessibleInterface for timeline cells – impl

#include "accessibletimelineview.h"
#include "timelinewidget.h"
#include "../core/timeline.h"
#include "../core/track.h"
#include "../core/clip.h"

#include <QAccessible>

namespace Thrive {

// =====================================================================
// Factory – registered by registerAccessibleTimelineFactory()
// =====================================================================

static QAccessibleInterface *timelineAccessibleFactory(const QString &cn,
                                                       QObject *obj)
{
    if (cn == QLatin1String("Thrive::TimelineWidget")
        || cn == QLatin1String("TimelineWidget")) {
        auto *w = qobject_cast<TimelineWidget *>(obj);
        if (w)
            return new AccessibleTimelineView(w);
    }
    return nullptr;
}

void registerAccessibleTimelineFactory()
{
    QAccessible::installFactory(timelineAccessibleFactory);
}

// =====================================================================
// AccessibleTimelineView (table)
// =====================================================================

AccessibleTimelineView::AccessibleTimelineView(TimelineWidget *widget)
    : QAccessibleWidget(widget, QAccessible::Table)
{
}

void *AccessibleTimelineView::interface_cast(QAccessible::InterfaceType type)
{
    if (type == QAccessible::TableInterface)
        return static_cast<QAccessibleTableInterface *>(this);
    return QAccessibleWidget::interface_cast(type);
}

QAccessible::Role AccessibleTimelineView::role() const
{
    return QAccessible::Table;
}

QString AccessibleTimelineView::text(QAccessible::Text t) const
{
    if (t == QAccessible::Name)
        return QObject::tr("Timeline");
    if (t == QAccessible::Description)
        return QObject::tr("Up and down arrow for tracks, left and right arrow for clips.");
    return {};
}

int AccessibleTimelineView::childCount() const
{
    const int r = rowCount();
    const int c = columnCount();
    return r * c;
}

QAccessibleInterface *AccessibleTimelineView::child(int index) const
{
    const int cols = columnCount();
    if (cols <= 0 || index < 0 || index >= childCount())
        return nullptr;
    const int row = index / cols;
    const int col = index % cols;
    return cellAt(row, col);
}

QAccessibleInterface *AccessibleTimelineView::focusChild() const
{
    auto *tl = timeline();
    auto *w  = qobject_cast<TimelineWidget *>(object());
    if (!tl || !w) return nullptr;
    return new AccessibleTimelineCell(w, tl->currentTrackIndex(),
                                      tl->currentClipIndex());
}

int AccessibleTimelineView::indexOfChild(const QAccessibleInterface *iface) const
{
    if (!iface)
        return -1;
    // Check if it's one of our cells by attempting a table-cell cast
    auto *cellIface = const_cast<QAccessibleInterface *>(iface);
    auto *cell = static_cast<QAccessibleTableCellInterface *>(
        cellIface->interface_cast(QAccessible::TableCellInterface));
    if (!cell)
        return -1;
    const int cols = columnCount();
    if (cols <= 0)
        return -1;
    return cell->rowIndex() * cols + cell->columnIndex();
}

Timeline *AccessibleTimelineView::timeline() const
{
    auto *w = qobject_cast<TimelineWidget *>(object());
    // We stored the Timeline pointer as a child of the widget — use
    // findChild.  Alternatively we could friend‐access the private member.
    return w ? w->timeline() : nullptr;
}

int AccessibleTimelineView::rowCount() const
{
    auto *tl = timeline();
    return tl ? tl->tracks().size() : 0;
}

int AccessibleTimelineView::columnCount() const
{
    auto *tl = timeline();
    if (!tl) return 0;
    int maxClips = 0;
    for (const auto *trk : tl->tracks())
        maxClips = qMax(maxClips, static_cast<int>(trk->clips().size()));
    return qMax(maxClips, 1);
}

QAccessibleInterface *AccessibleTimelineView::cellAt(int row, int column) const
{
    auto *w = qobject_cast<TimelineWidget *>(object());
    if (!w) return nullptr;
    return new AccessibleTimelineCell(w, row, column);
}

QString AccessibleTimelineView::rowDescription(int row) const
{
    auto *tl = timeline();
    if (!tl || row < 0 || row >= tl->tracks().size()) return {};
    const Track *trk = tl->tracks().at(row);
    return (trk->type() == Track::Type::Video)
               ? QObject::tr("Video track %1").arg(row + 1)
               : QObject::tr("Audio track %1").arg(row + 1);
}

QString AccessibleTimelineView::columnDescription(int column) const
{
    return QObject::tr("Clip %1").arg(column + 1);
}

// Selection helpers – single-cell selection model
int  AccessibleTimelineView::selectedCellCount()   const { return 1; }
int  AccessibleTimelineView::selectedColumnCount() const { return 0; }
int  AccessibleTimelineView::selectedRowCount()    const { return 0; }

QList<QAccessibleInterface *> AccessibleTimelineView::selectedCells() const
{
    auto *tl = timeline();
    auto *w  = qobject_cast<TimelineWidget *>(object());
    if (!tl || !w) return {};
    return { new AccessibleTimelineCell(w, tl->currentTrackIndex(),
                                        tl->currentClipIndex()) };
}

QList<int> AccessibleTimelineView::selectedColumns()  const { return {}; }
QList<int> AccessibleTimelineView::selectedRows()     const { return {}; }
bool AccessibleTimelineView::isColumnSelected(int)    const { return false; }
bool AccessibleTimelineView::isRowSelected(int)       const { return false; }
bool AccessibleTimelineView::selectRow(int)                 { return false; }
bool AccessibleTimelineView::selectColumn(int)              { return false; }
bool AccessibleTimelineView::unselectRow(int)               { return false; }
bool AccessibleTimelineView::unselectColumn(int)            { return false; }
QAccessibleInterface *AccessibleTimelineView::caption()  const { return nullptr; }
QAccessibleInterface *AccessibleTimelineView::summary()  const { return nullptr; }

void AccessibleTimelineView::modelChange(QAccessibleTableModelChangeEvent *)
{
    // No-op; we read live from the model.
}

// =====================================================================
// AccessibleTimelineCell
// =====================================================================

AccessibleTimelineCell::AccessibleTimelineCell(TimelineWidget *widget,
                                               int row, int col)
    : m_widget(widget), m_row(row), m_col(col)
{
}

bool AccessibleTimelineCell::isValid() const
{
    return m_widget != nullptr;
}

QObject *AccessibleTimelineCell::object() const
{
    return nullptr; // virtual cell, no backing QObject
}

QAccessibleInterface *AccessibleTimelineCell::parent() const
{
    return QAccessible::queryAccessibleInterface(m_widget);
}

QAccessible::Role AccessibleTimelineCell::role() const
{
    return QAccessible::Cell;
}

QAccessible::State AccessibleTimelineCell::state() const
{
    QAccessible::State s;
    s.selectable = true;
    s.focusable  = true;
    s.selected   = isSelected();
    s.focused    = isSelected();
    return s;
}

QRect AccessibleTimelineCell::rect() const
{
    // No meaningful visual rect – the widget is a flat label.
    return m_widget ? m_widget->geometry() : QRect();
}

QString AccessibleTimelineCell::text(QAccessible::Text t) const
{
    if (t != QAccessible::Name && t != QAccessible::Description)
        return {};

    auto *tl = m_widget ? m_widget->timeline() : nullptr;
    if (!tl) return {};

    const auto &tracks = tl->tracks();
    if (m_row < 0 || m_row >= tracks.size())
        return QObject::tr("Empty");

    const auto &clips = tracks.at(m_row)->clips();
    if (m_col < 0 || m_col >= clips.size())
        return QObject::tr("Empty");

    return clips.at(m_col)->accessibleSummary();
}

void *AccessibleTimelineCell::interface_cast(QAccessible::InterfaceType type)
{
    if (type == QAccessible::TableCellInterface)
        return static_cast<QAccessibleTableCellInterface *>(this);
    return nullptr;
}

bool AccessibleTimelineCell::isSelected() const
{
    auto *tl = m_widget ? m_widget->timeline() : nullptr;
    if (!tl) return false;
    return tl->currentTrackIndex() == m_row
        && tl->currentClipIndex()  == m_col;
}

QList<QAccessibleInterface *> AccessibleTimelineCell::rowHeaderCells() const
{
    return {};
}

QList<QAccessibleInterface *> AccessibleTimelineCell::columnHeaderCells() const
{
    return {};
}

QAccessibleInterface *AccessibleTimelineCell::table() const
{
    return QAccessible::queryAccessibleInterface(m_widget);
}

} // namespace Thrive
