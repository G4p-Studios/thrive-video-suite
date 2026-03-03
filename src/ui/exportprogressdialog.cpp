// SPDX-License-Identifier: MIT
// Thrive Video Suite – Export progress dialog implementation

#include "exportprogressdialog.h"
#include "../engine/renderengine.h"
#include "../accessibility/announcer.h"

#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QAccessible>

namespace Thrive {

ExportProgressDialog::ExportProgressDialog(RenderEngine *render,
                                           Announcer *announcer,
                                           QWidget *parent)
    : QDialog(parent)
    , m_render(render)
    , m_announcer(announcer)
{
    setWindowTitle(tr("Exporting…"));
    setAccessibleName(tr("Export progress"));
    setMinimumWidth(400);
    setModal(true);

    // Prevent closing via X while rendering
    setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);

    auto *layout = new QVBoxLayout(this);

    m_label = new QLabel(tr("Rendering: 0%"), this);
    m_label->setAccessibleName(tr("Export percentage"));
    layout->addWidget(m_label);

    m_bar = new QProgressBar(this);
    m_bar->setAccessibleName(tr("Export progress bar"));
    m_bar->setRange(0, 100);
    m_bar->setValue(0);
    layout->addWidget(m_bar);

    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_cancelBtn->setAccessibleName(tr("Cancel export"));
    layout->addWidget(m_cancelBtn);

    connect(m_cancelBtn, &QPushButton::clicked, this, [this]() {
        m_render->cancelRender();
        m_announcer->announce(tr("Export cancelled."),
                              Announcer::Priority::High);
        reject();
    });

    connect(m_render, &RenderEngine::renderProgress,
            this, &ExportProgressDialog::onProgress);
    connect(m_render, &RenderEngine::renderFinished,
            this, &ExportProgressDialog::onFinished);
}

void ExportProgressDialog::onProgress(int percent)
{
    m_bar->setValue(percent);
    m_label->setText(tr("Rendering: %1%").arg(percent));

    // Notify assistive technology of the value change
    if (auto *iface = QAccessible::queryAccessibleInterface(m_bar)) {
        QAccessibleValueChangeEvent ev(m_bar, percent);
        QAccessible::updateAccessibility(&ev);
    }

    // Announce at every 10% increment
    int bucket = (percent / 10) * 10;
    if (bucket > m_lastAnnounced && bucket > 0) {
        m_lastAnnounced = bucket;
        m_announcer->announce(
            tr("Export %1 percent complete.").arg(bucket),
            Announcer::Priority::Low);
    }
}

void ExportProgressDialog::onFinished(bool success)
{
    if (success) {
        m_announcer->announce(tr("Export complete!"),
                              Announcer::Priority::High);
        accept();
    } else {
        // Render was cancelled or failed – dialog may already be closed
        // from cancel button; only show error if still visible
        if (isVisible()) {
            m_announcer->announce(tr("Export failed."),
                                  Announcer::Priority::High);
            reject();
        }
    }
}

} // namespace Thrive
