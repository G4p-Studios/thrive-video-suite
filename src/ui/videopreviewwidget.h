// SPDX-License-Identifier: MIT
// Thrive Video Suite – Embedded video preview surface

#pragma once

#include <QWidget>

namespace Thrive {

/// A lightweight QWidget whose sole purpose is to provide a native
/// window handle (`winId()`) that the SDL2 consumer can render into.
///
/// The widget paints itself black when idle (no consumer connected)
/// and lets SDL draw over it during playback.
class VideoPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoPreviewWidget(QWidget *parent = nullptr);

    /// The native window ID that should be passed to the SDL2 consumer
    /// via the "window_id" property.
    [[nodiscard]] quintptr nativeWindowId();

    /// Recommended size for the preview area.
    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
};

} // namespace Thrive
