// SPDX-License-Identifier: MIT
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
    // Parse "HH:MM:SS:FF"
    const auto parts = text.split(u':');
    if (parts.size() != 4)
        return TimeCode(0, fps);

    const int h = parts[0].toInt();
    const int m = parts[1].toInt();
    const int s = parts[2].toInt();
    const int f = parts[3].toInt();

    const auto totalFrames = static_cast<int64_t>(
        ((h * 3600) + (m * 60) + s) * fps + f);
    return TimeCode(totalFrames, fps);
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
    return TimeCode(m_frame - other.m_frame, m_fps);
}

} // namespace Thrive
