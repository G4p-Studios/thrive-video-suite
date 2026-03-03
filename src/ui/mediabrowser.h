// SPDX-License-Identifier: MIT
// Thrive Video Suite – Media browser panel (import & manage source files)

#pragma once

#include <QWidget>
#include <QStringList>

QT_FORWARD_DECLARE_CLASS(QListWidget)
QT_FORWARD_DECLARE_CLASS(QPushButton)
QT_FORWARD_DECLARE_CLASS(QVBoxLayout)
QT_FORWARD_DECLARE_CLASS(QListWidgetItem)

namespace Thrive {

class Announcer;
class MltEngine;

/// A list of imported media files.  Users can browse, import, and
/// remove source files (video, audio, images) before placing them on
/// the timeline.  Every item carries an accessible label that is
/// spoken by the screen reader.
class MediaBrowser : public QWidget
{
    Q_OBJECT

public:
    explicit MediaBrowser(Announcer *announcer,
                          MltEngine *engine = nullptr,
                          QWidget *parent = nullptr);

    [[nodiscard]] QStringList files() const;

signals:
    /// Emitted when the user wants to place the selected file on the
    /// timeline.
    void fileActivated(const QString &filePath);

public slots:
    void importFiles();
    void removeSelected();

private slots:
    void onItemActivated(QListWidgetItem *item);
    void onCurrentChanged(QListWidgetItem *current, QListWidgetItem *previous);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    Announcer   *m_announcer = nullptr;
    MltEngine   *m_engine    = nullptr;
    QListWidget *m_list      = nullptr;
    QPushButton *m_btnImport = nullptr;
    QPushButton *m_btnRemove = nullptr;
    QVBoxLayout *m_layout    = nullptr;
};

} // namespace Thrive
