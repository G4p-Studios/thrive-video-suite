// SPDX-License-Identifier: MIT
// Thrive Video Suite – Effect definition

#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVector>

namespace Thrive {

/// Describes a single parameter of an effect (auto-generated from MLT metadata).
struct EffectParameter
{
    QString id;                ///< MLT property name (e.g. "brightness")
    QString displayName;       ///< tr()-wrapped human-readable name
    QString description;       ///< tr()-wrapped description for screen reader
    QString type;              ///< "float", "int", "bool", "string", "color"
    QVariant defaultValue;
    QVariant minimum;
    QVariant maximum;
    QVariant currentValue;
};

/// An instance of an effect (filter) applied to a clip or track.
class Effect : public QObject
{
    Q_OBJECT

public:
    explicit Effect(QObject *parent = nullptr);
    Effect(const QString &serviceId,
           const QString &displayName,
           const QString &description,
           QObject *parent = nullptr);

    [[nodiscard]] QString serviceId()   const { return m_serviceId; }
    [[nodiscard]] QString displayName() const { return m_displayName; }
    [[nodiscard]] QString description() const { return m_description; }
    [[nodiscard]] QString category()    const { return m_category; }
    [[nodiscard]] bool    isEnabled()   const { return m_enabled; }

    void setDisplayName(const QString &name);
    void setDescription(const QString &desc);
    void setCategory(const QString &cat);
    void setEnabled(bool enabled);

    // Parameters
    [[nodiscard]] const QVector<EffectParameter> &parameters() const { return m_parameters; }
    void addParameter(const EffectParameter &param);
    void setParameterValue(const QString &paramId, const QVariant &value);
    [[nodiscard]] QVariant parameterValue(const QString &paramId) const;

    /// Screen reader summary: "Brightness – Adjusts the brightness of the clip. Enabled."
    [[nodiscard]] QString accessibleSummary() const;

signals:
    void enabledChanged(bool enabled);
    void descriptionChanged(const QString &description);
    void parameterChanged(const QString &paramId, const QVariant &value);

private:
    QString m_serviceId;
    QString m_displayName;
    QString m_description;
    QString m_category;
    bool    m_enabled = true;
    QVector<EffectParameter> m_parameters;
};

} // namespace Thrive
