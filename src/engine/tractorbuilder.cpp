// SPDX-License-Identifier: MIT
// Thrive Video Suite – TractorBuilder implementation

#include "tractorbuilder.h"
#include "mltengine.h"

#include "../core/timeline.h"
#include "../core/track.h"
#include "../core/clip.h"
#include "../core/effect.h"
#include "../core/transition.h"
#include "../core/timecode.h"

#include <mlt++/MltFactory.h>
#include <mlt++/MltFilter.h>
#include <mlt++/MltPlaylist.h>
#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>
#include <mlt++/MltTractor.h>
#include <mlt++/MltTransition.h>

#include <mlt++/MltConsumer.h>

#include <QTemporaryFile>
#include <QFile>

#include <algorithm>

namespace Thrive {

TractorBuilder::TractorBuilder(MltEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
{
}

TractorBuilder::~TractorBuilder() = default;

// ---------------------------------------------------------------------------
// rebuild
// ---------------------------------------------------------------------------

bool TractorBuilder::rebuild(Timeline *timeline)
{
    if (!m_engine || !m_engine->isInitialized() || !timeline)
        return false;

    // Tear down previous build
    m_tractor.reset();
    m_producers.clear();
    m_playlists.clear();
    m_filters.clear();
    m_transitions.clear();

    auto *profile = m_engine->compositionProfile();

    const auto &tracks = timeline->tracks();
    if (tracks.isEmpty()) {
        // Nothing to build – create a minimal black tractor so the
        // consumer has something valid to connect to.
        m_tractor = std::make_unique<Mlt::Tractor>(*profile);
        m_tractor->set("title", "Thrive Timeline (empty)");
        emit tractorReady(m_tractor.get());
        return true;
    }

    // Create tractor – the default constructor gives us an empty tractor
    m_tractor = std::make_unique<Mlt::Tractor>(*profile);
    m_tractor->set("title", "Thrive Timeline");

    for (int i = 0; i < tracks.size(); ++i) {
        auto playlist = buildPlaylist(tracks.at(i));
        if (!playlist)
            continue;

        // set_track places the producer at the given index in the tractor
        m_tractor->set_track(*playlist, i);

        // Keep the playlist alive
        m_playlists.push_back(std::move(playlist));
    }

    emit tractorReady(m_tractor.get());
    return true;
}

// ---------------------------------------------------------------------------
// serializeToXml – export current Tractor as MLT XML
// ---------------------------------------------------------------------------

QByteArray TractorBuilder::serializeToXml() const
{
    if (!m_tractor || !m_engine || !m_engine->isInitialized())
        return QByteArray();

    auto *profile = m_engine->compositionProfile();

    // Use a temporary file because the MLT XML consumer writes to a resource
    QTemporaryFile tmpFile;
    tmpFile.setAutoRemove(true);
    if (!tmpFile.open())
        return QByteArray();

    const QByteArray tmpPath = tmpFile.fileName().toUtf8();
    tmpFile.close();

    Mlt::Consumer xmlConsumer(*profile, "xml", tmpPath.constData());
    if (!xmlConsumer.is_valid())
        return QByteArray();

    xmlConsumer.set("no_meta", 1);
    xmlConsumer.connect(*m_tractor);
    xmlConsumer.run();

    QFile f(QString::fromUtf8(tmpPath));
    if (!f.open(QIODevice::ReadOnly))
        return QByteArray();

    return f.readAll();
}

// ---------------------------------------------------------------------------
// buildPlaylist – one Track → one Mlt::Playlist
// ---------------------------------------------------------------------------

std::unique_ptr<Mlt::Playlist> TractorBuilder::buildPlaylist(Track *track)
{
    auto *profile = m_engine->compositionProfile();
    auto playlist = std::make_unique<Mlt::Playlist>(*profile);

    if (!track)
        return playlist;

    // Sort clips by timeline position before inserting
    QVector<Clip *> sortedClips = track->clips();
    std::sort(sortedClips.begin(), sortedClips.end(),
              [](const Clip *a, const Clip *b) {
                  return a->timelinePosition().frame() <
                         b->timelinePosition().frame();
              });

    int64_t currentFrame = 0;

    for (Clip *clip : sortedClips) {
        auto producer = buildProducer(clip);
        if (!producer || !producer->is_valid())
            continue;

        // Insert blank space if there is a gap between the current position
        // and where this clip starts on the timeline.
        const int64_t clipStart = clip->timelinePosition().frame();
        if (clipStart > currentFrame) {
            const int gapLength = static_cast<int>(clipStart - currentFrame);
            playlist->blank(gapLength - 1);   // blank length is 0-based in MLT
        }

        // Set source in/out points on the producer
        const int inFrame  = static_cast<int>(clip->inPoint().frame());
        const int outFrame = static_cast<int>(clip->outPoint().frame());

        if (outFrame > inFrame) {
            producer->set_in_and_out(inFrame, outFrame);
        }

        // Attach clip-level effects
        for (Effect *effect : clip->effects()) {
            if (effect && effect->isEnabled())
                attachEffect(*producer, effect);
        }

        // Append the producer to the playlist
        const int clipLength = producer->get_out() - producer->get_in() + 1;
        playlist->append(*producer, producer->get_in(), producer->get_out());

        currentFrame = clipStart + clipLength;

        // Keep producer alive
        m_producers.push_back(std::move(producer));
    }

    // ── Apply within-playlist transitions (mix) ──────────────────────
    // Walk the sorted clips looking for outTransition on clip N or
    // inTransition on clip N+1.  When found, use Playlist::mix() to
    // create an overlap region, then attach an Mlt::Transition to it.
    // mix() merges the two playlist entries at the given clip index,
    // so we process from the end to avoid index shifts.
    {
        // Build a list of (playlistClipIndex, duration, serviceId) for each
        // transition.  playlist clip indices correspond to the sorted order
        // but we must account for blanks inserted earlier.  We track the
        // playlist clip index as we go.
        struct MixInfo { int playlistClipIdx; int durationFrames; QString serviceId; };
        QVector<MixInfo> mixes;

        // Map sortedClips index → playlist clip index (blanks shift indices)
        int pIdx = 0; // running playlist clip index
        QVector<int> clipPlaylistIdx;
        int64_t curFrame = 0;
        for (int i = 0; i < sortedClips.size(); ++i) {
            const int64_t cs = sortedClips[i]->timelinePosition().frame();
            if (cs > curFrame)
                ++pIdx; // there was a blank entry before this clip
            clipPlaylistIdx.append(pIdx);
            const int in  = static_cast<int>(sortedClips[i]->inPoint().frame());
            const int out = static_cast<int>(sortedClips[i]->outPoint().frame());
            curFrame = cs + (out - in + 1);
            ++pIdx;
        }

        for (int i = 0; i + 1 < sortedClips.size(); ++i) {
            Transition *trans = sortedClips[i]->outTransition();
            if (!trans)
                trans = sortedClips[i + 1]->inTransition();
            if (!trans)
                continue;

            MixInfo mi;
            mi.playlistClipIdx = clipPlaylistIdx[i];
            mi.durationFrames  = static_cast<int>(trans->duration().frame());
            mi.serviceId       = trans->serviceId();
            if (mi.durationFrames <= 0)
                continue;
            mixes.append(mi);
        }

        // Apply mixes from the end so earlier indices stay valid
        for (int m = mixes.size() - 1; m >= 0; --m) {
            const auto &mi = mixes[m];
            auto mltTrans = std::make_unique<Mlt::Transition>(
                *profile, mi.serviceId.toUtf8().constData());
            if (!mltTrans->is_valid())
                continue;

            // Playlist::mix() accepts an optional Transition* to apply
            // to the mix region it creates.
            playlist->mix(mi.playlistClipIdx, mi.durationFrames,
                          mltTrans.get());
            m_transitions.push_back(std::move(mltTrans));
        }
    }

    // Track-level effects → attach as filters on the entire playlist
    for (Effect *effect : track->trackEffects()) {
        if (effect && effect->isEnabled())
            attachEffect(*playlist, effect);
    }

    // If the track is muted, attach a volume filter set to 0
    if (track->isMuted()) {
        auto muteFilter = std::make_unique<Mlt::Filter>(
            *m_engine->compositionProfile(), "volume");
        if (muteFilter->is_valid()) {
            muteFilter->set("gain", 0.0);
            playlist->attach(*muteFilter);
            m_filters.push_back(std::move(muteFilter));
        }
    }

    return playlist;
}

// ---------------------------------------------------------------------------
// buildProducer – one Clip → one Mlt::Producer
// ---------------------------------------------------------------------------

std::unique_ptr<Mlt::Producer> TractorBuilder::buildProducer(Clip *clip)
{
    if (!clip)
        return nullptr;

    auto *profile = m_engine->compositionProfile();
    const QByteArray path = clip->sourcePath().toUtf8();

    auto producer = std::make_unique<Mlt::Producer>(
        *profile, path.constData());

    if (!producer->is_valid())
        return nullptr;

    // Store readable title for debugging / MLT XML output
    producer->set("title", clip->name().toUtf8().constData());

    return producer;
}

// ---------------------------------------------------------------------------
// attachEffect – one Effect → one Mlt::Filter
// ---------------------------------------------------------------------------

void TractorBuilder::attachEffect(Mlt::Producer &producer, Effect *effect)
{
    if (!effect)
        return;

    auto *profile = m_engine->compositionProfile();
    const QByteArray sid = effect->serviceId().toUtf8();

    auto filter = std::make_unique<Mlt::Filter>(*profile, sid.constData());
    if (!filter->is_valid())
        return;

    // Apply all parameter values
    for (const auto &param : effect->parameters()) {
        if (param.currentValue.isValid()) {
            filter->set(param.id.toUtf8().constData(),
                        param.currentValue.toString().toUtf8().constData());
        }
    }

    producer.attach(*filter);
    m_filters.push_back(std::move(filter));
}

} // namespace Thrive
