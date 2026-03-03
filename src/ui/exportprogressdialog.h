// SPDX-License-Identifier: MIT
// Thrive Video Suite – Export progress dialog

#pragma once

#include <QDialog>

QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QProgressBar)
QT_FORWARD_DECLARE_CLASS(QPushButton)

namespace Thrive {

class Announcer;
class RenderEngine;

/// Modal progress dialog shown during export.  Accessible: progress
/// percentage is announced at every 10% increment.
class ExportProgressDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportProgressDialog(RenderEngine *render,
                                  Announcer *announcer,
                                  QWidget *parent = nullptr);

public slots:
    void onProgress(int percent);
    void onFinished(bool success);

private:
    RenderEngine *m_render    = nullptr;
    Announcer    *m_announcer = nullptr;
    QProgressBar *m_bar       = nullptr;
    QLabel       *m_label     = nullptr;
    QPushButton  *m_cancelBtn = nullptr;
    int           m_lastAnnounced = -1;
};

} // namespace Thrive
