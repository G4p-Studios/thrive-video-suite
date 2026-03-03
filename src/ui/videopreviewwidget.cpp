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

    // Ensure a native window handle is created immediately so the
    // SDL2 consumer has a valid target before the first paint.
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);

    // Dark background while nothing is playing
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setAutoFillBackground(true);
    setPalette(pal);

    // Minimum size so the dock cannot be squashed to zero
    setMinimumSize(320, 180);

    setFocusPolicy(Qt::ClickFocus);
}

quintptr VideoPreviewWidget::nativeWindowId()
{
    // winId() implicitly creates the native window if it doesn't exist
    return static_cast<quintptr>(winId());
}

QSize VideoPreviewWidget::sizeHint() const
{
    // 16:9 at a comfortable preview size
    return {640, 360};
}

void VideoPreviewWidget::paintEvent(QPaintEvent * /*event*/)
{
    // Paint black when the consumer is not rendering (e.g. stopped state).
    // Once SDL2 takes over this window, it paints its own frames and
    // our paintEvent is effectively a no-op.
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
}

} // namespace Thrive
