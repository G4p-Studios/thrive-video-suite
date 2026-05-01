// SPDX-License-Identifier: MIT
// Thrive Video Suite - Stack template serialization

#include "stacktemplate.h"

#include <QJsonValue>

namespace Thrive {

bool StackTemplate::isValid() const
{
    return !id.trimmed().isEmpty()
        && !name.trimmed().isEmpty()
        && totalSeconds > 0.0
        && overlayStartSeconds >= 0.0
        && captionStartSeconds >= 0.0
        && secondaryStartSeconds >= 0.0
        && fadeSeconds > 0.0;
}

QJsonObject StackTemplate::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), id);
    obj.insert(QStringLiteral("name"), name);
    obj.insert(QStringLiteral("description"), description);
    obj.insert(QStringLiteral("captionDefaultText"), captionDefaultText);
    obj.insert(QStringLiteral("secondaryPhaseName"), secondaryPhaseName);
    obj.insert(QStringLiteral("secondaryDefaultText"), secondaryDefaultText);
    obj.insert(QStringLiteral("includeSecondaryByDefault"), includeSecondaryByDefault);
    obj.insert(QStringLiteral("totalSeconds"), totalSeconds);
    obj.insert(QStringLiteral("overlayStartSeconds"), overlayStartSeconds);
    obj.insert(QStringLiteral("captionStartSeconds"), captionStartSeconds);
    obj.insert(QStringLiteral("secondaryStartSeconds"), secondaryStartSeconds);
    obj.insert(QStringLiteral("fadeSeconds"), fadeSeconds);
    return obj;
}

bool StackTemplate::fromJson(const QJsonObject &obj,
                             StackTemplate *out,
                             QString *error)
{
    if (!out) {
        if (error)
            *error = QStringLiteral("Output template pointer is null.");
        return false;
    }

    StackTemplate t;
    t.id = obj.value(QStringLiteral("id")).toString().trimmed();
    t.name = obj.value(QStringLiteral("name")).toString().trimmed();
    t.description = obj.value(QStringLiteral("description")).toString().trimmed();

    t.captionDefaultText = obj.value(QStringLiteral("captionDefaultText"))
                               .toString(QStringLiteral("WARNER BROS."));
    t.secondaryPhaseName = obj.value(QStringLiteral("secondaryPhaseName"))
                               .toString(QStringLiteral("Second text phase"));
    t.secondaryDefaultText = obj.value(QStringLiteral("secondaryDefaultText"))
                                 .toString(QStringLiteral("LOONEY TUNES"));
    t.includeSecondaryByDefault = obj.value(QStringLiteral("includeSecondaryByDefault"))
                                      .toBool(true);

    t.totalSeconds = obj.value(QStringLiteral("totalSeconds")).toDouble(6.0);
    t.overlayStartSeconds = obj.value(QStringLiteral("overlayStartSeconds")).toDouble(0.6);
    t.captionStartSeconds = obj.value(QStringLiteral("captionStartSeconds")).toDouble(1.2);
    t.secondaryStartSeconds = obj.value(QStringLiteral("secondaryStartSeconds")).toDouble(3.5);
    t.fadeSeconds = obj.value(QStringLiteral("fadeSeconds")).toDouble(0.8);

    if (!t.isValid()) {
        if (error)
            *error = QStringLiteral("Template has invalid or missing required fields.");
        return false;
    }

    *out = t;
    return true;
}

StackTemplate StackTemplate::builtInLooneyTunes()
{
    StackTemplate t;
    t.id = QStringLiteral("builtin.looney_tunes");
    t.name = QStringLiteral("Looney Tunes Intro");
    t.description = QStringLiteral("Classic rings, shield, caption, and second text phase.");
    t.captionDefaultText = QStringLiteral("WARNER BROS.");
    t.secondaryPhaseName = QStringLiteral("Looney text");
    t.secondaryDefaultText = QStringLiteral("LOONEY TUNES");
    t.includeSecondaryByDefault = true;
    t.totalSeconds = 6.0;
    t.overlayStartSeconds = 0.6;
    t.captionStartSeconds = 1.2;
    t.secondaryStartSeconds = 3.5;
    t.fadeSeconds = 0.8;
    return t;
}

} // namespace Thrive
