// SPDX-License-Identifier: MIT
// Thrive Video Suite – Timecode utility

#pragma once

#include <QCoreApplication>
#include <QString>
#include <compare>
#include <cstdint>

namespace Thrive {

/// Represents a point in time on the timeline, stored as a frame number.
/// Provides human-readable formatting (HH:MM:SS:FF) for screen reader output.
class TimeCode
{
    Q_DECLARE_TR_FUNCTIONS(TimeCode)

public:
    TimeCode() = default;
    explicit TimeCode(int64_t frame, double fps = 25.0);

    static TimeCode fromSeconds(double seconds, double fps = 25.0);
    static TimeCode fromString(const QString &text, double fps = 25.0);

    [[nodiscard]] int64_t  frame()   const { return m_frame; }
    [[nodiscard]] double   seconds() const;
    [[nodiscard]] double   fps()     const { return m_fps; }

    [[nodiscard]] int hours()   const;
    [[nodiscard]] int minutes() const;
    [[nodiscard]] int secs()    const;
    [[nodiscard]] int frames()  const;

    /// "01:23:45:12"
    [[nodiscard]] QString toString() const;

    /// Screen-reader friendly: "1 hour, 23 minutes, 45 seconds, 12 frames"
    [[nodiscard]] QString toSpokenString() const;

    // Arithmetic
    TimeCode  operator+(const TimeCode &other) const;
    TimeCode  operator-(const TimeCode &other) const;

    // Comparison – based on frame position only (fps is metadata)
    bool operator==(const TimeCode &other) const { return m_frame == other.m_frame; }
    std::strong_ordering operator<=>(const TimeCode &other) const { return m_frame <=> other.m_frame; }

private:
    int64_t m_frame = 0;
    double  m_fps   = 25.0;
};

} // namespace Thrive
