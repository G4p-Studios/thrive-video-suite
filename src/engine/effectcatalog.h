// SPDX-License-Identifier: MIT
// Thrive Video Suite – Effect/transition catalog from MLT repository

#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QHash>

namespace Thrive {

class MltEngine;

/// Describes one MLT service available for use (filter or transition).
struct CatalogEntry
{
    QString serviceId;       ///< MLT service name (e.g. "brightness")
    QString displayName;     ///< tr()-wrapped human-readable name
    QString description;     ///< tr()-wrapped description for screen reader
    QString category;        ///< "Video", "Audio", "Transition", etc.
    QString type;            ///< "filter" or "transition"
    bool    isHidden = false;

    /// Screen reader: "Brightness (Video filter) – Adjusts clip brightness"
    [[nodiscard]] QString accessibleSummary() const;
};

/// Enumerates all available MLT filters and transitions, queries their
/// metadata, and provides a searchable catalog with accessible descriptions.
class EffectCatalog : public QObject
{
    Q_OBJECT

public:
    explicit EffectCatalog(MltEngine *engine, QObject *parent = nullptr);

    /// Scan the MLT repository and populate the catalog.
    void refresh();

    [[nodiscard]] const QVector<CatalogEntry> &filters()     const { return m_filters; }
    [[nodiscard]] const QVector<CatalogEntry> &transitions() const { return m_transitions; }

    /// All entries (filters + transitions) combined.
    [[nodiscard]] QVector<CatalogEntry> allEntries() const;

    /// Search by name or description (case-insensitive substring match).
    [[nodiscard]] QVector<CatalogEntry> search(const QString &query) const;

    /// Look up a single entry by service ID.
    [[nodiscard]] const CatalogEntry *findByServiceId(const QString &id) const;

private:
    void populateFilters();
    void populateTransitions();
    [[nodiscard]] QString curatedDisplayName(const QString &serviceId) const;
    [[nodiscard]] QString curatedDescription(const QString &serviceId) const;

    MltEngine *m_engine = nullptr;
    QVector<CatalogEntry> m_filters;
    QVector<CatalogEntry> m_transitions;
    QHash<QString, CatalogEntry> m_index; // serviceId → entry
};

} // namespace Thrive
