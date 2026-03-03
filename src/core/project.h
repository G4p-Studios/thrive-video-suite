// SPDX-License-Identifier: MIT
// Thrive Video Suite – Project definition

#pragma once

#include <QObject>
#include <QString>

namespace Thrive {

class Timeline;

/// Top-level project model. Owns the timeline and project-level settings.
class Project : public QObject
{
    Q_OBJECT

public:
    explicit Project(QObject *parent = nullptr);

    [[nodiscard]] QString  name()     const { return m_name; }
    [[nodiscard]] QString  filePath() const { return m_filePath; }
    [[nodiscard]] bool     isModified() const { return m_modified; }
    [[nodiscard]] Timeline *timeline() const { return m_timeline; }

    // Project metadata
    [[nodiscard]] double   fps()    const { return m_fps; }
    [[nodiscard]] int      width()  const { return m_width; }
    [[nodiscard]] int      height() const { return m_height; }

    void setName(const QString &name);
    void setFilePath(const QString &path);
    void setModified(bool modified);
    void setFps(double fps);
    void setResolution(int width, int height);

    // Preferences stored in project
    [[nodiscard]] bool scrubAudioEnabled() const { return m_scrubAudio; }
    [[nodiscard]] int  previewScale()      const { return m_previewScale; }

    void setScrubAudioEnabled(bool enabled);
    void setPreviewScale(int height);

    /// Create a fresh empty project with default settings
    void reset();

signals:
    void nameChanged(const QString &name);
    void modifiedChanged(bool modified);
    void settingsChanged();
    void timelineAboutToChange();

private:
    QString   m_name;
    QString   m_filePath;
    bool      m_modified = false;
    Timeline *m_timeline = nullptr;

    double m_fps    = 25.0;
    int    m_width  = 1920;
    int    m_height = 1080;

    bool m_scrubAudio   = true;
    int  m_previewScale = 640;
};

} // namespace Thrive
