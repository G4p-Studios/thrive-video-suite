// SPDX-License-Identifier: MIT
// Thrive Video Suite – .tvs project file serializer implementation

#include "projectserializer.h"
#include "project.h"
#include "timeline.h"
#include "track.h"
#include "clip.h"
#include "effect.h"
#include "marker.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QIODevice>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

namespace Thrive {

ProjectSerializer::ProjectSerializer(QObject *parent)
    : QObject(parent)
{
}

// ---------------------------------------------------------------------------
// Save
// ---------------------------------------------------------------------------
bool ProjectSerializer::save(const Project *project, const QString &filePath)
{
    QuaZip zip(filePath);
    if (!zip.open(QuaZip::mdCreate)) {
        m_lastError = tr("Cannot create file: %1").arg(filePath);
        return false;
    }

    emit saveProgress(10);

    // Write metadata.json
    {
        QuaZipFile metaFile(&zip);
        if (!metaFile.open(QIODevice::WriteOnly,
                           QuaZipNewInfo(QStringLiteral("metadata.json")))) {
            m_lastError = tr("Cannot write metadata.json");
            zip.close();
            return false;
        }
        metaFile.write(buildMetadataJson(project));
        metaFile.close();
    }

    emit saveProgress(50);

    // Write project.mlt (placeholder – full MLT XML serialisation via engine)
    {
        QuaZipFile mltFile(&zip);
        if (!mltFile.open(QIODevice::WriteOnly,
                          QuaZipNewInfo(QStringLiteral("project.mlt")))) {
            m_lastError = tr("Cannot write project.mlt");
            zip.close();
            return false;
        }
        mltFile.write(buildMltXml(project));
        mltFile.close();
    }

    emit saveProgress(90);
    zip.close();
    emit saveProgress(100);

    return true;
}

// ---------------------------------------------------------------------------
// Load
// ---------------------------------------------------------------------------
bool ProjectSerializer::load(Project *project, const QString &filePath)
{
    QuaZip zip(filePath);
    if (!zip.open(QuaZip::mdUnzip)) {
        m_lastError = tr("Cannot open file: %1").arg(filePath);
        return false;
    }

    emit loadProgress(10);

    // Read metadata.json
    if (zip.setCurrentFile(QStringLiteral("metadata.json"))) {
        QuaZipFile metaFile(&zip);
        if (metaFile.open(QIODevice::ReadOnly)) {
            if (!applyMetadataJson(project, metaFile.readAll())) {
                zip.close();
                return false;
            }
            metaFile.close();
        }
    }

    emit loadProgress(50);

    // Read project.mlt
    if (zip.setCurrentFile(QStringLiteral("project.mlt"))) {
        QuaZipFile mltFile(&zip);
        if (mltFile.open(QIODevice::ReadOnly)) {
            if (!applyMltXml(project, mltFile.readAll())) {
                zip.close();
                return false;
            }
            mltFile.close();
        }
    }

    emit loadProgress(90);
    zip.close();

    project->setFilePath(filePath);
    project->setModified(false);

    emit loadProgress(100);
    return true;
}

// ---------------------------------------------------------------------------
// JSON metadata
// ---------------------------------------------------------------------------
QByteArray ProjectSerializer::buildMetadataJson(const Project *project) const
{
    QJsonObject root;
    root[QStringLiteral("version")]  = QStringLiteral("1.0");
    root[QStringLiteral("name")]     = project->name();
    root[QStringLiteral("fps")]      = project->fps();
    root[QStringLiteral("width")]    = project->width();
    root[QStringLiteral("height")]   = project->height();
    root[QStringLiteral("scrubAudio")]   = project->scrubAudioEnabled();
    root[QStringLiteral("previewScale")] = project->previewScale();

    // Track metadata
    QJsonArray tracksArr;
    const auto *timeline = project->timeline();
    for (const auto *track : timeline->tracks()) {
        QJsonObject tObj;
        tObj[QStringLiteral("name")] = track->name();
        tObj[QStringLiteral("type")] = (track->type() == Track::Type::Video)
                                           ? QStringLiteral("video")
                                           : QStringLiteral("audio");
        tObj[QStringLiteral("muted")]  = track->isMuted();
        tObj[QStringLiteral("locked")] = track->isLocked();

        // Clip metadata (descriptions, user notes)
        QJsonArray clipsArr;
        for (const auto *clip : track->clips()) {
            QJsonObject cObj;
            cObj[QStringLiteral("name")]        = clip->name();
            cObj[QStringLiteral("description")] = clip->description();
            cObj[QStringLiteral("source")]      = clip->sourcePath();
            cObj[QStringLiteral("in")]          = clip->inPoint().toString();
            cObj[QStringLiteral("out")]         = clip->outPoint().toString();

            QJsonArray effectsArr;
            for (const auto *effect : clip->effects()) {
                QJsonObject eObj;
                eObj[QStringLiteral("serviceId")]   = effect->serviceId();
                eObj[QStringLiteral("displayName")] = effect->displayName();
                eObj[QStringLiteral("description")] = effect->description();
                eObj[QStringLiteral("enabled")]     = effect->isEnabled();

                QJsonArray paramsArr;
                for (const auto &p : effect->parameters()) {
                    QJsonObject pObj;
                    pObj[QStringLiteral("id")]    = p.id;
                    pObj[QStringLiteral("value")] = p.currentValue.toString();
                    paramsArr.append(pObj);
                }
                eObj[QStringLiteral("parameters")] = paramsArr;
                effectsArr.append(eObj);
            }
            cObj[QStringLiteral("effects")] = effectsArr;
            clipsArr.append(cObj);
        }
        tObj[QStringLiteral("clips")] = clipsArr;
        tracksArr.append(tObj);
    }
    root[QStringLiteral("tracks")] = tracksArr;

    // Markers
    QJsonArray markersArr;
    for (const auto *marker : timeline->markers()) {
        QJsonObject mObj;
        mObj[QStringLiteral("name")]     = marker->name();
        mObj[QStringLiteral("position")] = marker->position().toString();
        mObj[QStringLiteral("comment")]  = marker->comment();
        markersArr.append(mObj);
    }
    root[QStringLiteral("markers")] = markersArr;

    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool ProjectSerializer::applyMetadataJson(Project *project,
                                           const QByteArray &json)
{
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(json, &err);
    if (doc.isNull()) {
        m_lastError = tr("Invalid metadata.json: %1").arg(err.errorString());
        return false;
    }

    const auto root = doc.object();
    project->setName(root[QStringLiteral("name")].toString());
    project->setFps(root[QStringLiteral("fps")].toDouble(25.0));
    project->setResolution(root[QStringLiteral("width")].toInt(1920),
                           root[QStringLiteral("height")].toInt(1080));
    project->setScrubAudioEnabled(root[QStringLiteral("scrubAudio")].toBool(true));
    project->setPreviewScale(root[QStringLiteral("previewScale")].toInt(640));

    const double fps = project->fps();
    auto *timeline = project->timeline();

    // Rebuild tracks from metadata
    // (The MLT XML will rebuild the engine graph; metadata restores names & descriptions)
    const auto tracksArr = root[QStringLiteral("tracks")].toArray();
    for (const auto &tVal : tracksArr) {
        const auto tObj = tVal.toObject();
        const auto type = tObj[QStringLiteral("type")].toString() == QStringLiteral("video")
                              ? Track::Type::Video
                              : Track::Type::Audio;
        auto *track = new Track(tObj[QStringLiteral("name")].toString(), type);
        track->setMuted(tObj[QStringLiteral("muted")].toBool());
        track->setLocked(tObj[QStringLiteral("locked")].toBool());

        const auto clipsArr = tObj[QStringLiteral("clips")].toArray();
        for (const auto &cVal : clipsArr) {
            const auto cObj = cVal.toObject();
            auto *clip = new Clip(
                cObj[QStringLiteral("name")].toString(),
                cObj[QStringLiteral("source")].toString(),
                TimeCode::fromString(cObj[QStringLiteral("in")].toString(), fps),
                TimeCode::fromString(cObj[QStringLiteral("out")].toString(), fps));
            clip->setDescription(cObj[QStringLiteral("description")].toString());

            const auto effectsArr = cObj[QStringLiteral("effects")].toArray();
            for (const auto &eVal : effectsArr) {
                const auto eObj = eVal.toObject();
                auto *effect = new Effect(
                    eObj[QStringLiteral("serviceId")].toString(),
                    eObj[QStringLiteral("displayName")].toString(),
                    eObj[QStringLiteral("description")].toString());
                effect->setEnabled(eObj[QStringLiteral("enabled")].toBool(true));

                const auto paramsArr = eObj[QStringLiteral("parameters")].toArray();
                for (const auto &pVal : paramsArr) {
                    const auto pObj = pVal.toObject();
                    effect->setParameterValue(pObj[QStringLiteral("id")].toString(),
                                              pObj[QStringLiteral("value")]);
                }
                clip->addEffect(effect);
            }
            track->addClip(clip);
        }
        timeline->addTrack(track);
    }

    // Markers
    const auto markersArr = root[QStringLiteral("markers")].toArray();
    for (const auto &mVal : markersArr) {
        const auto mObj = mVal.toObject();
        auto *marker = new Marker(
            mObj[QStringLiteral("name")].toString(),
            TimeCode::fromString(mObj[QStringLiteral("position")].toString(), fps),
            mObj[QStringLiteral("comment")].toString());
        timeline->addMarker(marker);
    }

    return true;
}

// ---------------------------------------------------------------------------
// MLT XML (stub – real implementation delegates to MltEngine)
// ---------------------------------------------------------------------------
QByteArray ProjectSerializer::buildMltXml(const Project * /*project*/) const
{
    // TODO: Delegate to MltEngine to serialize the live Mlt::Tractor to XML
    //       via Mlt::Consumer with "xml" target.
    return QByteArrayLiteral("<?xml version=\"1.0\"?>\n<mlt/>\n");
}

bool ProjectSerializer::applyMltXml(Project * /*project*/,
                                     const QByteArray & /*xml*/)
{
    // TODO: Delegate to MltEngine to deserialize XML via Mlt::Producer("xml-string")
    return true;
}

} // namespace Thrive
