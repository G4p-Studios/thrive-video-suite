// SPDX-License-Identifier: MIT
// Thrive Video Suite – Project implementation

#include "project.h"
#include "timeline.h"
#include "track.h"

namespace Thrive {

Project::Project(QObject *parent)
    : QObject(parent)
    , m_timeline(new Timeline(this))
{
    reset();
}

void Project::setName(const QString &name)
{
    if (m_name != name) {
        m_name = name;
        emit nameChanged(m_name);
    }
}

void Project::setFilePath(const QString &path)
{
    m_filePath = path;
}

void Project::setModified(bool modified)
{
    if (m_modified != modified) {
        m_modified = modified;
        emit modifiedChanged(m_modified);
    }
}

void Project::setFps(double fps)
{
    m_fps = fps;
    emit settingsChanged();
}

void Project::setResolution(int width, int height)
{
    m_width  = width;
    m_height = height;
    emit settingsChanged();
}

void Project::setScrubAudioEnabled(bool enabled)
{
    m_scrubAudio = enabled;
    emit settingsChanged();
}

void Project::setPreviewScale(int height)
{
    m_previewScale = height;
    emit settingsChanged();
}

void Project::reset()
{
    m_name     = tr("Untitled Project");
    m_filePath.clear();
    m_modified = false;
    m_fps      = 25.0;
    m_width    = 1920;
    m_height   = 1080;
    m_scrubAudio   = true;
    m_previewScale = 640;

    // Warn observers to disconnect from the old Timeline
    emit timelineAboutToChange();

    // Disconnect all signals from the old timeline so deferred
    // slot invocations don't dereference a dead pointer
    if (m_timeline) {
        m_timeline->disconnect();
        delete m_timeline;
    }
    m_timeline = new Timeline(this);

    // Create default tracks
    m_timeline->addTrack(new Track(tr("Video 1"), Track::Type::Video));
    m_timeline->addTrack(new Track(tr("Audio 1"), Track::Type::Audio));

    emit nameChanged(m_name);
    emit modifiedChanged(false);
    emit settingsChanged();
}

} // namespace Thrive
