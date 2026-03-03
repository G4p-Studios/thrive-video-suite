// SPDX-License-Identifier: MIT
// Thrive Video Suite – Export settings dialog implementation

#include "exportdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace Thrive {

ExportDialog::ExportDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Export Video"));
    setAccessibleName(tr("Export settings"));
    setMinimumWidth(450);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;

    // Output path
    auto *pathRow = new QHBoxLayout;
    m_outputPath = new QLineEdit(this);
    m_outputPath->setAccessibleName(tr("Output file path"));
    m_outputPath->setPlaceholderText(tr("Choose output file…"));
    pathRow->addWidget(m_outputPath);
    auto *browseBtn = new QPushButton(tr("Browse…"), this);
    browseBtn->setAccessibleName(tr("Browse for output file"));
    connect(browseBtn, &QPushButton::clicked,
            this, &ExportDialog::browseOutput);
    pathRow->addWidget(browseBtn);
    form->addRow(tr("&Output:"), pathRow);

    // Format
    m_formatCombo = new QComboBox(this);
    m_formatCombo->setAccessibleName(tr("Container format"));
    m_formatCombo->addItem(tr("MP4"), QStringLiteral("mp4"));
    m_formatCombo->addItem(tr("MKV"), QStringLiteral("matroska"));
    m_formatCombo->addItem(tr("WebM"), QStringLiteral("webm"));
    m_formatCombo->addItem(tr("MOV"), QStringLiteral("mov"));
    m_formatCombo->addItem(tr("AVI"), QStringLiteral("avi"));
    form->addRow(tr("&Format:"), m_formatCombo);

    // Video codec
    m_vcodecCombo = new QComboBox(this);
    m_vcodecCombo->setAccessibleName(tr("Video codec"));
    m_vcodecCombo->addItem(tr("H.264 (libx264)"), QStringLiteral("libx264"));
    m_vcodecCombo->addItem(tr("H.265 (libx265)"), QStringLiteral("libx265"));
    m_vcodecCombo->addItem(tr("VP9"),             QStringLiteral("libvpx-vp9"));
    m_vcodecCombo->addItem(tr("MPEG-4"),          QStringLiteral("mpeg4"));
    form->addRow(tr("&Video codec:"), m_vcodecCombo);

    // Audio codec
    m_acodecCombo = new QComboBox(this);
    m_acodecCombo->setAccessibleName(tr("Audio codec"));
    m_acodecCombo->addItem(tr("AAC"),   QStringLiteral("aac"));
    m_acodecCombo->addItem(tr("Opus"),  QStringLiteral("libopus"));
    m_acodecCombo->addItem(tr("Vorbis"), QStringLiteral("libvorbis"));
    m_acodecCombo->addItem(tr("MP3"),   QStringLiteral("libmp3lame"));
    m_acodecCombo->addItem(tr("PCM (WAV)"), QStringLiteral("pcm_s16le"));
    form->addRow(tr("&Audio codec:"), m_acodecCombo);

    // Video bitrate
    m_vBitrate = new QSpinBox(this);
    m_vBitrate->setAccessibleName(tr("Video bitrate kilobits per second"));
    m_vBitrate->setRange(0, 100000);
    m_vBitrate->setSingleStep(500);
    m_vBitrate->setSpecialValueText(tr("Default"));
    m_vBitrate->setValue(0);
    m_vBitrate->setSuffix(tr(" kbps"));
    form->addRow(tr("Video &bitrate:"), m_vBitrate);

    // Audio bitrate
    m_aBitrate = new QSpinBox(this);
    m_aBitrate->setAccessibleName(tr("Audio bitrate kilobits per second"));
    m_aBitrate->setRange(0, 640);
    m_aBitrate->setSingleStep(32);
    m_aBitrate->setSpecialValueText(tr("Default"));
    m_aBitrate->setValue(0);
    m_aBitrate->setSuffix(tr(" kbps"));
    form->addRow(tr("Audio b&itrate:"), m_aBitrate);

    // Resolution override
    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setAccessibleName(tr("Output width pixels"));
    m_widthSpin->setRange(0, 7680);
    m_widthSpin->setSingleStep(2);
    m_widthSpin->setSpecialValueText(tr("Source"));
    m_widthSpin->setValue(0);
    form->addRow(tr("&Width:"), m_widthSpin);

    m_heightSpin = new QSpinBox(this);
    m_heightSpin->setAccessibleName(tr("Output height pixels"));
    m_heightSpin->setRange(0, 4320);
    m_heightSpin->setSingleStep(2);
    m_heightSpin->setSpecialValueText(tr("Source"));
    m_heightSpin->setValue(0);
    form->addRow(tr("&Height:"), m_heightSpin);

    layout->addLayout(form);

    // Buttons
    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Export"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void ExportDialog::browseOutput()
{
    const QString filter =
        tr("MP4 Video (*.mp4);;MKV Video (*.mkv);;WebM Video (*.webm);;"
           "MOV Video (*.mov);;AVI Video (*.avi);;All Files (*)");
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export Video"), QString(), filter);
    if (!path.isEmpty())
        m_outputPath->setText(path);
}

QString ExportDialog::outputPath() const { return m_outputPath->text(); }
QString ExportDialog::format()     const { return m_formatCombo->currentData().toString(); }
QString ExportDialog::videoCodec() const { return m_vcodecCombo->currentData().toString(); }
QString ExportDialog::audioCodec() const { return m_acodecCombo->currentData().toString(); }
int     ExportDialog::videoBitrate() const { return m_vBitrate->value(); }
int     ExportDialog::audioBitrate() const { return m_aBitrate->value(); }
int     ExportDialog::width()  const { return m_widthSpin->value(); }
int     ExportDialog::height() const { return m_heightSpin->value(); }

} // namespace Thrive
