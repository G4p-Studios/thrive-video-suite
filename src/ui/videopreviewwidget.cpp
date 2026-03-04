// SPDX-License-Identifier: MIT
// Thrive Video Suite – Embedded video preview surface implementation

#include "videopreviewwidget.h"

#include <QPainter>
#include <QPaintEvent>

namespace Thrive {

VideoPreviewWidget::VideoPreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("VideoPreviewWidget"));
    setAccessibleName(tr("Video Preview"));
    setAccessibleDescription(
        tr("Displays a preview of the timeline video output."));

    setAttribute(Qt::WA_OpaquePaintEvent);

    // Dark background while nothing is playing
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setAutoFillBackground(true);
    setPalette(pal);

    // Minimum size so the dock cannot be squashed to zero
    setMinimumSize(320, 180);

    setFocusPolicy(Qt::ClickFocus);
}

void VideoPreviewWidget::updateFrame(const QImage &frame)
{
    m_currentFrame = frame;
    update();
}

QSize VideoPreviewWidget::sizeHint() const
{
    // 16:9 at a comfortable preview size
    return {640, 360};
}

void VideoPreviewWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);

    if (!m_currentFrame.isNull()) {
        // Scale the frame to fit the widget while preserving aspect ratio
        const QSize scaled = m_currentFrame.size().scaled(
            size(), Qt::KeepAspectRatio);
        const int x = (width()  - scaled.width())  / 2;
        const int y = (height() - scaled.height()) / 2;
        p.drawImage(QRect(x, y, scaled.width(), scaled.height()),
                    m_currentFrame);
    }
}

} // namespace Thrive
