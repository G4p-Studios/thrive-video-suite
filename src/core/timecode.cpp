// SPDX-License-Identifier: GPL-3.0-or-later
// Thrive Video Suite – Timecode utility

#include "timecode.h"
#include <cmath>

namespace Thrive {

TimeCode::TimeCode(int64_t frame, double fps)
    : m_frame(frame), m_fps(fps)
{
}

TimeCode TimeCode::fromSeconds(double seconds, double fps)
{
    return TimeCode(static_cast<int64_t>(std::round(seconds * fps)), fps);
}

TimeCode TimeCode::fromString(const QString &text, double fps)
{
    TimeCode result;
    return tryParse(text, &result, fps) ? result : TimeCode(0, fps);
}

bool TimeCode::tryParse(const QString &text, TimeCode *result, double fps)
{
    if (!result || fps <= 0.0)
        return false;

    // Parse and validate "HH:MM:SS:FF".
    const auto parts = text.split(u':');
    if (parts.size() != 4)
        return false;

    bool hOk = false;
    bool mOk = false;
    bool sOk = false;
    bool fOk = false;
    const int h = parts[0].toInt(&hOk);
    const int m = parts[1].toInt(&mOk);
    const int s = parts[2].toInt(&sOk);
    const int f = parts[3].toInt(&fOk);

    const int framesPerSecond = static_cast<int>(std::ceil(fps));
    if (!hOk || !mOk || !sOk || !fOk || h < 0
        || m < 0 || m > 59 || s < 0 || s > 59
        || f < 0 || f >= framesPerSecond)
        return false;

    const auto totalFrames = static_cast<int64_t>(
        ((h * 3600) + (m * 60) + s) * fps + f);
    *result = TimeCode(totalFrames, fps);
    return true;
}

double TimeCode::seconds() const
{
    return static_cast<double>(m_frame) / m_fps;
}

int TimeCode::hours() const
{
    const auto totalSeconds = static_cast<int>(seconds());
    return totalSeconds / 3600;
}

int TimeCode::minutes() const
{
    const auto totalSeconds = static_cast<int>(seconds());
    return (totalSeconds % 3600) / 60;
}

int TimeCode::secs() const
{
    const auto totalSeconds = static_cast<int>(seconds());
    return totalSeconds % 60;
}

int TimeCode::frames() const
{
    return static_cast<int>(m_frame % static_cast<int64_t>(m_fps));
}

QString TimeCode::toString() const
{
    return QStringLiteral("%1:%2:%3:%4")
        .arg(hours(),   2, 10, QChar(u'0'))
        .arg(minutes(), 2, 10, QChar(u'0'))
        .arg(secs(),    2, 10, QChar(u'0'))
        .arg(frames(),  2, 10, QChar(u'0'));
}

QString TimeCode::toSpokenString() const
{
    QStringList parts;

    if (hours() > 0)
        parts << tr("%n hour(s)", nullptr, hours());
    if (minutes() > 0)
        parts << tr("%n minute(s)", nullptr, minutes());
    if (secs() > 0)
        parts << tr("%n second(s)", nullptr, secs());
    if (frames() > 0 || parts.isEmpty())
        parts << tr("%n frame(s)", nullptr, frames());

    return parts.join(QStringLiteral(", "));
}

TimeCode TimeCode::operator+(const TimeCode &other) const
{
    return TimeCode(m_frame + other.m_frame, m_fps);
}

TimeCode TimeCode::operator-(const TimeCode &other) const
{
    const auto result = m_frame - other.m_frame;
    return TimeCode(result > 0 ? result : 0, m_fps);
}

} // namespace Thrive
