// SPDX-License-Identifier: MIT
// Thrive Video Suite - Stack template definition

#pragma once

#include <QJsonObject>
#include <QString>

namespace Thrive {

struct StackTemplate
{
    QString id;
    QString name;
    QString description;

    QString captionDefaultText;
    QString secondaryPhaseName;
    QString secondaryDefaultText;
    bool includeSecondaryByDefault = true;

    double totalSeconds = 6.0;
    double overlayStartSeconds = 0.6;
    double captionStartSeconds = 1.2;
    double secondaryStartSeconds = 3.5;
    double fadeSeconds = 0.8;

    bool isValid() const;

    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject &obj,
                         StackTemplate *out,
                         QString *error = nullptr);

    static StackTemplate builtInLooneyTunes();
};

} // namespace Thrive
