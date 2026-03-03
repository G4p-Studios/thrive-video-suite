// SPDX-License-Identifier: MIT
// Thrive Video Suite – Effect implementation

#include "effect.h"

namespace Thrive {

Effect::Effect(QObject *parent)
    : QObject(parent)
{
}

Effect::Effect(const QString &serviceId,
               const QString &displayName,
               const QString &description,
               QObject *parent)
    : QObject(parent)
    , m_serviceId(serviceId)
    , m_displayName(displayName)
    , m_description(description)
{
}

void Effect::setDisplayName(const QString &name)
{
    m_displayName = name;
}

void Effect::setDescription(const QString &desc)
{
    if (m_description != desc) {
        m_description = desc;
        emit descriptionChanged(m_description);
    }
}

void Effect::setCategory(const QString &cat)
{
    m_category = cat;
}

void Effect::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        emit enabledChanged(m_enabled);
    }
}

void Effect::addParameter(const EffectParameter &param)
{
    m_parameters.append(param);
}

void Effect::setParameterValue(const QString &paramId, const QVariant &value)
{
    for (auto &p : m_parameters) {
        if (p.id == paramId) {
            p.currentValue = value;
            emit parameterChanged(paramId, value);
            return;
        }
    }
}

QVariant Effect::parameterValue(const QString &paramId) const
{
    for (const auto &p : m_parameters) {
        if (p.id == paramId)
            return p.currentValue;
    }
    return {};
}

QString Effect::accessibleSummary() const
{
    //: Screen reader summary for an effect. %1=display name, %2=description, %3=enabled/disabled
    return tr("%1 – %2. %3.")
        .arg(m_displayName,
             m_description,
             m_enabled ? tr("Enabled") : tr("Disabled"));
}

} // namespace Thrive
