// SPDX-License-Identifier: MIT
// Thrive Video Suite – .tvs project file serializer implementation

#include "projectserializer.h"
#include "project.h"
#include "timeline.h"
#include "track.h"
#include "clip.h"
#include "effect.h"
#include "marker.h"
#include "transition.h"

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

        // Track-level effects
        QJsonArray trackEffectsArr;
        for (const auto *tfx : track->trackEffects()) {
            QJsonObject tfxObj;
            tfxObj[QStringLiteral("serviceId")]   = tfx->serviceId();
            tfxObj[QStringLiteral("displayName")] = tfx->displayName();
            tfxObj[QStringLiteral("description")] = tfx->description();
            tfxObj[QStringLiteral("enabled")]     = tfx->isEnabled();

            QJsonArray tfxParamsArr;
            for (const auto &p : tfx->parameters()) {
                QJsonObject pObj;
                pObj[QStringLiteral("id")]    = p.id;
                pObj[QStringLiteral("value")] = p.currentValue.toString();
                tfxParamsArr.append(pObj);
            }
            tfxObj[QStringLiteral("parameters")] = tfxParamsArr;
            trackEffectsArr.append(tfxObj);
        }
        tObj[QStringLiteral("trackEffects")] = trackEffectsArr;

        // Clip metadata (descriptions, user notes)
        QJsonArray clipsArr;
        for (const auto *clip : track->clips()) {
            QJsonObject cObj;
            cObj[QStringLiteral("name")]        = clip->name();
            cObj[QStringLiteral("description")] = clip->description();
            cObj[QStringLiteral("source")]      = clip->sourcePath();
            cObj[QStringLiteral("in")]          = clip->inPoint().toString();
            cObj[QStringLiteral("out")]         = clip->outPoint().toString();
            cObj[QStringLiteral("timelinePosition")] = clip->timelinePosition().toString();

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

            // Transitions
            if (const auto *inT = clip->inTransition()) {
                QJsonObject tObj;
                tObj[QStringLiteral("serviceId")]   = inT->serviceId();
                tObj[QStringLiteral("displayName")] = inT->displayName();
                tObj[QStringLiteral("description")] = inT->description();
                tObj[QStringLiteral("duration")]     = inT->duration().toString();
                cObj[QStringLiteral("inTransition")] = tObj;
            }
            if (const auto *outT = clip->outTransition()) {
                QJsonObject tObj;
                tObj[QStringLiteral("serviceId")]   = outT->serviceId();
                tObj[QStringLiteral("displayName")] = outT->displayName();
                tObj[QStringLiteral("description")] = outT->description();
                tObj[QStringLiteral("duration")]     = outT->duration().toString();
                cObj[QStringLiteral("outTransition")] = tObj;
            }
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

    // Clear existing timeline data to avoid duplicating tracks on re-load
    auto *timeline = project->timeline();
    while (timeline->trackCount() > 0) {
        auto *t = timeline->trackAt(timeline->trackCount() - 1);
        timeline->removeTrack(timeline->trackCount() - 1);
        delete t;
    }
    while (!timeline->markers().isEmpty()) {
        auto *m = timeline->markers().last();
        timeline->removeMarker(timeline->markers().size() - 1);
        delete m;
    }

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

        // Track-level effects
        const auto trackEffectsArr = tObj[QStringLiteral("trackEffects")].toArray();
        for (const auto &tfxVal : trackEffectsArr) {
            const auto tfxObj = tfxVal.toObject();
            auto *tfx = new Effect(
                tfxObj[QStringLiteral("serviceId")].toString(),
                tfxObj[QStringLiteral("displayName")].toString(),
                tfxObj[QStringLiteral("description")].toString());
            tfx->setEnabled(tfxObj[QStringLiteral("enabled")].toBool(true));

            const auto tfxParamsArr = tfxObj[QStringLiteral("parameters")].toArray();
            for (const auto &pVal : tfxParamsArr) {
                const auto pObj = pVal.toObject();
                const QString paramId = pObj[QStringLiteral("id")].toString();
                const QVariant paramVal = pObj[QStringLiteral("value")].toVariant();
                if (tfx->parameterValue(paramId).isValid()) {
                    tfx->setParameterValue(paramId, paramVal);
                } else {
                    EffectParameter ep;
                    ep.id = paramId;
                    ep.currentValue = paramVal;
                    tfx->addParameter(ep);
                }
            }
            track->addTrackEffect(tfx);
        }

        const auto clipsArr = tObj[QStringLiteral("clips")].toArray();
        for (const auto &cVal : clipsArr) {
            const auto cObj = cVal.toObject();
            auto *clip = new Clip(
                cObj[QStringLiteral("name")].toString(),
                cObj[QStringLiteral("source")].toString(),
                TimeCode::fromString(cObj[QStringLiteral("in")].toString(), fps),
                TimeCode::fromString(cObj[QStringLiteral("out")].toString(), fps));
            clip->setDescription(cObj[QStringLiteral("description")].toString());
            clip->setTimelinePosition(
                TimeCode::fromString(cObj[QStringLiteral("timelinePosition")].toString(), fps));

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
                    const QString paramId = pObj[QStringLiteral("id")].toString();
                    const QVariant paramVal = pObj[QStringLiteral("value")].toVariant();
                    // If the parameter already exists (e.g. from MLT metadata),
                    // update its value; otherwise create a minimal entry.
                    if (effect->parameterValue(paramId).isValid()) {
                        effect->setParameterValue(paramId, paramVal);
                    } else {
                        EffectParameter ep;
                        ep.id = paramId;
                        ep.currentValue = paramVal;
                        effect->addParameter(ep);
                    }
                }
                clip->addEffect(effect);
            }

            // Transitions
            if (cObj.contains(QStringLiteral("inTransition"))) {
                const auto tObj = cObj[QStringLiteral("inTransition")].toObject();
                auto *trans = new Transition(
                    tObj[QStringLiteral("serviceId")].toString(),
                    tObj[QStringLiteral("displayName")].toString(),
                    tObj[QStringLiteral("description")].toString(),
                    TimeCode::fromString(tObj[QStringLiteral("duration")].toString(), fps));
                clip->setInTransition(trans);
            }
            if (cObj.contains(QStringLiteral("outTransition"))) {
                const auto tObj = cObj[QStringLiteral("outTransition")].toObject();
                auto *trans = new Transition(
                    tObj[QStringLiteral("serviceId")].toString(),
                    tObj[QStringLiteral("displayName")].toString(),
                    tObj[QStringLiteral("description")].toString(),
                    TimeCode::fromString(tObj[QStringLiteral("duration")].toString(), fps));
                clip->setOutTransition(trans);
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
// MLT XML – uses cached XML from TractorBuilder::serializeToXml()
// ---------------------------------------------------------------------------
QByteArray ProjectSerializer::buildMltXml(const Project * /*project*/) const
{
    // Return the cached MLT XML if the caller supplied one via setMltXml().
    // This XML is produced by TractorBuilder::serializeToXml() which uses
    // the MLT "xml" consumer to serialise the live Mlt::Tractor.
    if (!m_cachedMltXml.isEmpty())
        return m_cachedMltXml;

    // Fallback: minimal valid MLT XML so the file is still well-formed.
    return QByteArrayLiteral("<?xml version=\"1.0\"?>\n<mlt/>\n");
}

bool ProjectSerializer::applyMltXml(Project * /*project*/,
                                     const QByteArray & /*xml*/)
{
    // The metadata.json is the authoritative data source.  After loading,
    // MainWindow calls rebuildTractor() which recreates the MLT pipeline
    // from the Timeline model, so we don't need to parse the MLT XML here.
    return true;
}

} // namespace Thrive
