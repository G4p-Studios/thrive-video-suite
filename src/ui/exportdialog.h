// SPDX-License-Identifier: MIT
// Thrive Video Suite – Export settings dialog

#pragma once

#include <QDialog>
#include <QString>

QT_FORWARD_DECLARE_CLASS(QComboBox)
QT_FORWARD_DECLARE_CLASS(QSpinBox)
QT_FORWARD_DECLARE_CLASS(QLineEdit)
QT_FORWARD_DECLARE_CLASS(QPushButton)
QT_FORWARD_DECLARE_CLASS(QLabel)

namespace Thrive {

/// Modal dialog for choosing export format, codecs, and quality.
class ExportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportDialog(QWidget *parent = nullptr);

    [[nodiscard]] QString outputPath() const;
    [[nodiscard]] QString format()     const;
    [[nodiscard]] QString videoCodec() const;
    [[nodiscard]] QString audioCodec() const;
    [[nodiscard]] int     videoBitrate() const; // kbps, 0 = default
    [[nodiscard]] int     audioBitrate() const; // kbps, 0 = default
    [[nodiscard]] int     width()  const;       // 0 = source
    [[nodiscard]] int     height() const;       // 0 = source

private slots:
    void browseOutput();

private:
    QLineEdit *m_outputPath  = nullptr;
    QComboBox *m_formatCombo = nullptr;
    QComboBox *m_vcodecCombo = nullptr;
    QComboBox *m_acodecCombo = nullptr;
    QSpinBox  *m_vBitrate    = nullptr;
    QSpinBox  *m_aBitrate    = nullptr;
    QSpinBox  *m_widthSpin   = nullptr;
    QSpinBox  *m_heightSpin  = nullptr;
};

} // namespace Thrive
