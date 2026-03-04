// SPDX-License-Identifier: MIT
// Thrive Video Suite – Visual timeline canvas with thumbnail strips

#pragma once

#include <QWidget>
#include <QScrollBar>

namespace Thrive {

class Timeline;
class ThumbnailProvider;
class MltEngine;

/// Visual canvas that renders a professional NLE-style timeline:
///   – Track header labels on the left
///   – Clip rectangles sized proportionally to their duration
///   – Video thumbnail strips inside clip rectangles
///   – Playhead line
///   – Focused clip highlight
///
/// This widget is embedded inside TimelineWidget alongside the
/// existing status label and keyboard navigation.
class TimelineCanvas : public QWidget
{
    Q_OBJECT

public:
    explicit TimelineCanvas(Timeline *timeline,
                            ThumbnailProvider *thumbs,
                            QWidget *parent = nullptr);

    void setTimeline(Timeline *timeline);

    // Layout constants (pixels)
    static constexpr int TrackHeight     = 70;
    static constexpr int TrackGap        = 2;
    static constexpr int HeaderWidth     = 80;
    static constexpr int RulerHeight     = 24;
    static constexpr int ThumbPadding    = 2;

    /// Pixels per frame.  Higher = more zoomed in.
    void setPixelsPerFrame(double ppf);
    [[nodiscard]] double pixelsPerFrame() const { return m_ppf; }

    /// Zoom in / out around the playhead.
    void zoomIn();
    void zoomOut();

    /// Horizontal scroll offset (pixels).
    void setScrollX(int x);
    [[nodiscard]] int scrollX() const { return m_scrollX; }

    /// Total canvas width (pixels) based on timeline duration.
    [[nodiscard]] int totalWidth() const;

signals:
    void scrollXChanged(int x);

public slots:
    void refresh();

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    void drawRuler(QPainter &p, const QRect &clip);
    void drawTracks(QPainter &p, const QRect &clip);
    void drawPlayhead(QPainter &p, const QRect &clip);

    Timeline          *m_timeline = nullptr;
    ThumbnailProvider *m_thumbs   = nullptr;

    double m_ppf     = 4.0;   // pixels per frame
    int    m_scrollX = 0;
};

} // namespace Thrive
