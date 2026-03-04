// SPDX-License-Identifier: MIT
// Thrive Video Suite – Embedded video preview surface

#pragma once

#include <QImage>
#include <QWidget>

namespace Thrive {

/// Displays video preview frames received from the PlaybackController.
///
/// The sdl2_audio consumer handles audio; video frames arrive via the
/// consumer-frame-show event and are painted here by Qt.
class VideoPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoPreviewWidget(QWidget *parent = nullptr);

    /// Recommended size for the preview area.
    [[nodiscard]] QSize sizeHint() const override;

public slots:
    /// Receive a decoded video frame for display.
    void updateFrame(const QImage &frame);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage m_currentFrame;
};

} // namespace Thrive
