// SPDX-License-Identifier: MIT
// Thrive Video Suite – .tvs project file serializer

#pragma once

#include <QObject>
#include <QString>

namespace Thrive {

class Project;

/// Reads/writes .tvs project files (ZIP containers with project.mlt + metadata.json).
class ProjectSerializer : public QObject
{
    Q_OBJECT

public:
    explicit ProjectSerializer(QObject *parent = nullptr);

    /// Save project to a .tvs file. Returns true on success.
    [[nodiscard]] bool save(const Project *project, const QString &filePath);

    /// Load project from a .tvs file. Returns true on success.
    [[nodiscard]] bool load(Project *project, const QString &filePath);

    /// Store MLT XML data to be included in the next save.
    /// Call this before save() with the output of TractorBuilder::serializeToXml().
    void setMltXml(const QByteArray &xml) { m_cachedMltXml = xml; }

    /// Last error message (if save/load failed).
    [[nodiscard]] QString lastError() const { return m_lastError; }

signals:
    void saveProgress(int percent);
    void loadProgress(int percent);

private:
    [[nodiscard]] QByteArray buildMetadataJson(const Project *project) const;
    bool applyMetadataJson(Project *project, const QByteArray &json);

    [[nodiscard]] QByteArray buildMltXml(const Project *project) const;
    bool applyMltXml(Project *project, const QByteArray &xml);

    QString    m_lastError;
    QByteArray m_cachedMltXml;
};

} // namespace Thrive
