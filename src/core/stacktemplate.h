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
    bool includeAudioByDefault = false;

    double totalSeconds = 6.0;
    double overlayStartSeconds = 0.6;
    double captionStartSeconds = 1.2;
    double secondaryStartSeconds = 3.5;
    double audioStartSeconds = 0.0;
    double fadeSeconds = 0.8;

    bool isValid() const;

    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject &obj,
                         StackTemplate *out,
                         QString *error = nullptr);

    static StackTemplate builtInLooneyTunes();
    static StackTemplate builtInPbs1971();
    static StackTemplate builtInPbs1984();
};

} // namespace Thrive
