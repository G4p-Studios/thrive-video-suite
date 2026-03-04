// SPDX-License-Identifier: MIT
// Thrive Video Suite – Visual timeline canvas implementation

#include "timelinecanvas.h"
#include "thumbnailprovider.h"
#include "../core/timeline.h"
#include "../core/track.h"
#include "../core/clip.h"
#include "../core/timecode.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QWheelEvent>
#include <QFontMetrics>

#include <algorithm>
#include <cmath>

namespace Thrive {

TimelineCanvas::TimelineCanvas(Timeline *timeline,
                               ThumbnailProvider *thumbs,
                               QWidget *parent)
    : QWidget(parent)
    , m_timeline(timeline)
    , m_thumbs(thumbs)
{
    setMinimumHeight(RulerHeight + TrackHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAccessibleName(tr("Timeline canvas"));

    if (m_timeline) {
        connect(m_timeline, &Timeline::tracksChanged, this, [this]() { update(); });
        connect(m_timeline, &Timeline::playheadChanged, this, [this]() { update(); });
        connect(m_timeline, &Timeline::currentTrackChanged, this, [this]() { update(); });
        connect(m_timeline, &Timeline::currentClipChanged, this, [this](int,int) { update(); });
    }

    if (m_thumbs) {
        connect(m_thumbs, &ThumbnailProvider::thumbnailReady,
                this, [this]() { update(); });
    }
}

void TimelineCanvas::setTimeline(Timeline *timeline)
{
    if (m_timeline)
        m_timeline->disconnect(this);
    m_timeline = timeline;
    if (m_timeline) {
        connect(m_timeline, &Timeline::tracksChanged, this, [this]() { update(); });
        connect(m_timeline, &Timeline::playheadChanged, this, [this]() { update(); });
        connect(m_timeline, &Timeline::currentTrackChanged, this, [this]() { update(); });
        connect(m_timeline, &Timeline::currentClipChanged, this, [this](int,int) { update(); });
    }
    update();
}

void TimelineCanvas::setPixelsPerFrame(double ppf)
{
    m_ppf = std::clamp(ppf, 0.1, 40.0);
    update();
}

void TimelineCanvas::zoomIn()
{
    setPixelsPerFrame(m_ppf * 1.3);
}

void TimelineCanvas::zoomOut()
{
    setPixelsPerFrame(m_ppf / 1.3);
}

void TimelineCanvas::setScrollX(int x)
{
    m_scrollX = std::max(0, x);
    update();
    emit scrollXChanged(m_scrollX);
}

int TimelineCanvas::totalWidth() const
{
    if (!m_timeline) return 0;
    return HeaderWidth + static_cast<int>(m_timeline->totalDuration().frame() * m_ppf) + 200;
}

void TimelineCanvas::refresh()
{
    update();
}

QSize TimelineCanvas::sizeHint() const
{
    int tracks = m_timeline ? m_timeline->trackCount() : 2;
    int h = RulerHeight + tracks * (TrackHeight + TrackGap) + TrackGap;
    return {800, h};
}

QSize TimelineCanvas::minimumSizeHint() const
{
    return {400, RulerHeight + TrackHeight + TrackGap * 2};
}

// ── painting ────────────────────────────────────────────────────────

void TimelineCanvas::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRect clip = event->rect();

    // Background
    p.fillRect(rect(), QColor(30, 30, 30));

    drawRuler(p, clip);
    drawTracks(p, clip);
    drawPlayhead(p, clip);
}

// ── ruler ───────────────────────────────────────────────────────────

void TimelineCanvas::drawRuler(QPainter &p, const QRect & /*clip*/)
{
    const QRect rulerRect(0, 0, width(), RulerHeight);
    p.fillRect(rulerRect, QColor(45, 45, 45));

    p.setPen(QColor(180, 180, 180));
    QFont f = font();
    f.setPointSize(8);
    p.setFont(f);

    if (!m_timeline) return;

    const double fps = m_timeline->playheadPosition().fps();
    if (fps <= 0) return;

    // Decide tick interval in frames based on zoom level
    // We want roughly one label every 60-120 pixels
    double secondsPerPixel = 1.0 / (m_ppf * fps);
    double labelIntervalSec = 1.0;
    if (secondsPerPixel * 80 > 1.0)
        labelIntervalSec = std::pow(10.0, std::ceil(std::log10(secondsPerPixel * 80)));
    else if (secondsPerPixel * 80 > 0.5)
        labelIntervalSec = 1.0;
    else if (secondsPerPixel * 80 > 0.1)
        labelIntervalSec = 0.5;

    int labelIntervalFrames = std::max(1, static_cast<int>(labelIntervalSec * fps));

    // Find first visible frame
    int firstFrame = static_cast<int>(m_scrollX / m_ppf);
    int startFrame = (firstFrame / labelIntervalFrames) * labelIntervalFrames;

    for (int frame = startFrame; ; frame += labelIntervalFrames) {
        int x = HeaderWidth + static_cast<int>(frame * m_ppf) - m_scrollX;
        if (x > width()) break;
        if (x < HeaderWidth) continue;

        // Tick mark
        p.drawLine(x, RulerHeight - 6, x, RulerHeight);

        // Time label
        int totalSec = static_cast<int>(frame / fps);
        int min = totalSec / 60;
        int sec = totalSec % 60;
        int fr  = frame % static_cast<int>(fps);
        QString label = QStringLiteral("%1:%2:%3")
            .arg(min, 2, 10, QLatin1Char('0'))
            .arg(sec, 2, 10, QLatin1Char('0'))
            .arg(fr, 2, 10, QLatin1Char('0'));
        p.drawText(x + 2, RulerHeight - 8, label);
    }

    // Divider line
    p.setPen(QColor(60, 60, 60));
    p.drawLine(0, RulerHeight - 1, width(), RulerHeight - 1);
}

// ── tracks & clips ──────────────────────────────────────────────────

void TimelineCanvas::drawTracks(QPainter &p, const QRect & /*clip*/)
{
    if (!m_timeline) return;

    const int trackCount = m_timeline->trackCount();
    const int currentTrack = m_timeline->currentTrackIndex();
    const int currentClip  = m_timeline->currentClipIndex();

    for (int t = 0; t < trackCount; ++t) {
        const Track *track = m_timeline->trackAt(t);
        if (!track) continue;

        const int trackY = RulerHeight + TrackGap + t * (TrackHeight + TrackGap);

        // Track background
        QColor bgColor = (track->type() == Track::Type::Video)
                             ? QColor(40, 45, 55) : QColor(40, 55, 45);
        p.fillRect(0, trackY, width(), TrackHeight, bgColor);

        // Track header
        QRect headerRect(0, trackY, HeaderWidth, TrackHeight);
        p.fillRect(headerRect, QColor(50, 50, 55));
        p.setPen(QColor(200, 200, 200));
        QFont hf = font();
        hf.setPointSize(9);
        hf.setBold(t == currentTrack);
        p.setFont(hf);
        p.drawText(headerRect.adjusted(4, 0, -2, 0), Qt::AlignVCenter,
                   track->name());

        // Track type indicator
        QFont sf = font();
        sf.setPointSize(7);
        p.setFont(sf);
        p.setPen(QColor(120, 120, 130));
        p.drawText(headerRect.adjusted(4, 0, -2, -4), Qt::AlignBottom,
                   track->typeString());

        // Clips
        const auto &clips = track->clips();
        for (int c = 0; c < clips.size(); ++c) {
            const Clip *cl = clips.at(c);

            const int64_t startFrame = cl->timelinePosition().frame();
            const int64_t durFrames  = cl->duration().frame();
            if (durFrames <= 0) continue;

            const int clipX = HeaderWidth
                              + static_cast<int>(startFrame * m_ppf)
                              - m_scrollX;
            const int clipW = std::max(4, static_cast<int>(durFrames * m_ppf));

            // Cull clips outside visible area
            if (clipX + clipW < HeaderWidth || clipX > width())
                continue;

            QRect clipRect(clipX, trackY + 1, clipW, TrackHeight - 2);

            // Clip body
            bool isFocused = (t == currentTrack && c == currentClip);
            QColor clipColor = (track->type() == Track::Type::Video)
                                   ? QColor(58, 95, 148)
                                   : QColor(58, 130, 80);
            if (isFocused)
                clipColor = clipColor.lighter(140);

            p.setPen(Qt::NoPen);
            p.setBrush(clipColor);
            p.drawRoundedRect(clipRect, 4, 4);

            // ── Thumbnail strip for video clips ─────────────────
            if (track->type() == Track::Type::Video && m_thumbs
                && clipW > 20)
            {
                const int thumbH = TrackHeight - 2 * ThumbPadding - 16;
                // 16px reserved at top for clip name
                if (thumbH > 8) {
                    const int thumbW = static_cast<int>(
                        thumbH * 16.0 / 9.0); // assume 16:9
                    const int thumbY = trackY + 1 + 14;  // below name
                    const QSize thumbSize(thumbW, thumbH);

                    // Clip the drawing to the clip rectangle so thumbnails
                    // don't bleed out
                    p.save();
                    QPainterPath clipPath;
                    clipPath.addRoundedRect(clipRect, 4, 4);
                    p.setClipPath(clipPath);

                    // Draw thumbnails at regular intervals across the clip
                    const int inFrame = static_cast<int>(cl->inPoint().frame());
                    for (int px = clipX + ThumbPadding;
                         px < clipX + clipW - ThumbPadding;
                         px += thumbW + 1)
                    {
                        // Map pixel position back to source frame
                        double frac = static_cast<double>(px - clipX) / clipW;
                        int srcFrame = inFrame
                                       + static_cast<int>(frac * durFrames);

                        QPixmap thumb = m_thumbs->thumbnail(
                            cl->sourcePath(), srcFrame, thumbSize);

                        if (!thumb.isNull()) {
                            p.drawPixmap(px, thumbY, thumbW, thumbH, thumb);
                        } else {
                            // Placeholder while loading
                            p.fillRect(px, thumbY, thumbW, thumbH,
                                       QColor(50, 50, 60));
                        }
                    }

                    p.restore();
                }
            }

            // ── Audio waveform placeholder ──────────────────────
            if (track->type() == Track::Type::Audio && clipW > 20) {
                // Draw a simple placeholder waveform pattern
                p.save();
                QPainterPath clipPath;
                clipPath.addRoundedRect(clipRect, 4, 4);
                p.setClipPath(clipPath);

                p.setPen(QPen(QColor(100, 200, 130, 120), 1));
                int centerY = trackY + TrackHeight / 2;
                for (int px = clipX + 2; px < clipX + clipW - 2; px += 3) {
                    // Simple pseudo-random waveform based on position
                    int amp = (qHash(cl->sourcePath()) + px * 7) % (TrackHeight / 3);
                    p.drawLine(px, centerY - amp / 2, px, centerY + amp / 2);
                }
                p.restore();
            }

            // ── Clip name ───────────────────────────────────────
            {
                p.setPen(Qt::white);
                QFont nf = font();
                nf.setPointSize(8);
                nf.setBold(isFocused);
                p.setFont(nf);

                QRect nameRect(clipX + 4, trackY + 2,
                               clipW - 8, 14);
                const QString elidedName =
                    p.fontMetrics().elidedText(cl->name(), Qt::ElideRight,
                                              nameRect.width());
                p.drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                           elidedName);
            }

            // ── Focus border ────────────────────────────────────
            if (isFocused) {
                p.setPen(QPen(QColor(255, 200, 60), 2));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(clipRect.adjusted(1, 1, -1, -1), 3, 3);
            }
        }

        // Divider line below track
        p.setPen(QColor(55, 55, 55));
        p.drawLine(0, trackY + TrackHeight,
                   width(), trackY + TrackHeight);
    }
}

// ── playhead ────────────────────────────────────────────────────────

void TimelineCanvas::drawPlayhead(QPainter &p, const QRect & /*clip*/)
{
    if (!m_timeline) return;

    const int64_t frame = m_timeline->playheadPosition().frame();
    const int x = HeaderWidth + static_cast<int>(frame * m_ppf) - m_scrollX;

    if (x < HeaderWidth || x > width())
        return;

    // Playhead line
    p.setPen(QPen(QColor(255, 60, 60), 2));
    p.drawLine(x, 0, x, height());

    // Playhead triangle at the top
    QPainterPath tri;
    tri.moveTo(x - 6, 0);
    tri.lineTo(x + 6, 0);
    tri.lineTo(x, 8);
    tri.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 60, 60));
    p.drawPath(tri);
}

// ── wheel event for horizontal scroll ───────────────────────────────

void TimelineCanvas::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        // Ctrl+wheel = zoom
        if (event->angleDelta().y() > 0)
            zoomIn();
        else
            zoomOut();
    } else {
        // Regular wheel = horizontal scroll
        int delta = event->angleDelta().y();
        setScrollX(m_scrollX - delta);
    }
    event->accept();
}

} // namespace Thrive
