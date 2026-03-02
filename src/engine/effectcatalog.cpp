// SPDX-License-Identifier: MIT
// Thrive Video Suite – Effect catalog implementation

#include "effectcatalog.h"
#include "mltengine.h"

#include <mlt++/MltRepository.h>
#include <mlt++/MltProperties.h>
#include <mlt/framework/mlt_service.h>

namespace Thrive {

// ---------------------------------------------------------------------------
// CatalogEntry
// ---------------------------------------------------------------------------
QString CatalogEntry::accessibleSummary() const
{
    //: Screen reader summary for an available effect. %1=name, %2=category, %3=type, %4=desc
    return QObject::tr("%1 (%2 %3) – %4")
        .arg(displayName, category, type, description);
}

// ---------------------------------------------------------------------------
// Curated display names for common MLT services (tr()-wrapped)
// ---------------------------------------------------------------------------
static const QHash<QString, std::pair<const char *, const char *>> &curatedNames()
{
    static const QHash<QString, std::pair<const char *, const char *>> map = {
        // Filters – Video
        { QStringLiteral("brightness"),
          { QT_TR_NOOP("Brightness"),
            QT_TR_NOOP("Adjusts the overall brightness of the video clip.") } },
        { QStringLiteral("contrast"),
          { QT_TR_NOOP("Contrast"),
            QT_TR_NOOP("Adjusts the contrast between light and dark areas.") } },
        { QStringLiteral("saturation"),
          { QT_TR_NOOP("Saturation"),
            QT_TR_NOOP("Adjusts the intensity of colors in the video.") } },
        { QStringLiteral("fadeInBrightness"),
          { QT_TR_NOOP("Fade In (Brightness)"),
            QT_TR_NOOP("Gradually fades the clip in from black at the start.") } },
        { QStringLiteral("fadeOutBrightness"),
          { QT_TR_NOOP("Fade Out (Brightness)"),
            QT_TR_NOOP("Gradually fades the clip out to black at the end.") } },
        { QStringLiteral("crop"),
          { QT_TR_NOOP("Crop"),
            QT_TR_NOOP("Removes pixels from the edges of the video frame.") } },
        { QStringLiteral("mirror"),
          { QT_TR_NOOP("Mirror"),
            QT_TR_NOOP("Flips the video horizontally, creating a mirror image.") } },
        { QStringLiteral("invert"),
          { QT_TR_NOOP("Invert Colors"),
            QT_TR_NOOP("Inverts all colors in the video, creating a negative image.") } },
        { QStringLiteral("greyscale"),
          { QT_TR_NOOP("Greyscale"),
            QT_TR_NOOP("Converts the video to black and white by removing color information.") } },
        { QStringLiteral("sepia"),
          { QT_TR_NOOP("Sepia Tone"),
            QT_TR_NOOP("Applies a warm brownish tint for a vintage photographic look.") } },
        { QStringLiteral("blur"),
          { QT_TR_NOOP("Blur"),
            QT_TR_NOOP("Softens the image by blurring fine details.") } },
        { QStringLiteral("charcoal"),
          { QT_TR_NOOP("Charcoal Sketch"),
            QT_TR_NOOP("Makes the video look like a charcoal drawing.") } },
        { QStringLiteral("wave"),
          { QT_TR_NOOP("Wave Distortion"),
            QT_TR_NOOP("Applies a rippling wave effect to the video.") } },
        { QStringLiteral("affine"),
          { QT_TR_NOOP("Transform"),
            QT_TR_NOOP("Scales, rotates, and positions the video within the frame.") } },
        { QStringLiteral("resize"),
          { QT_TR_NOOP("Resize"),
            QT_TR_NOOP("Scales the video to a different resolution.") } },
        { QStringLiteral("watermark"),
          { QT_TR_NOOP("Watermark / Overlay"),
            QT_TR_NOOP("Overlays an image or text on top of the video.") } },

        // Filters – Audio
        { QStringLiteral("volume"),
          { QT_TR_NOOP("Volume"),
            QT_TR_NOOP("Adjusts the audio volume level.") } },
        { QStringLiteral("panner"),
          { QT_TR_NOOP("Pan"),
            QT_TR_NOOP("Controls the left-right positioning of audio in stereo.") } },
        { QStringLiteral("fadeInVolume"),
          { QT_TR_NOOP("Fade In (Audio)"),
            QT_TR_NOOP("Gradually increases the audio volume from silence at the start.") } },
        { QStringLiteral("fadeOutVolume"),
          { QT_TR_NOOP("Fade Out (Audio)"),
            QT_TR_NOOP("Gradually decreases the audio volume to silence at the end.") } },
        { QStringLiteral("sox"),
          { QT_TR_NOOP("SoX Audio Effect"),
            QT_TR_NOOP("Applies an audio effect using the SoX sound processing library.") } },

        // Transitions
        { QStringLiteral("luma"),
          { QT_TR_NOOP("Cross Dissolve"),
            QT_TR_NOOP("Gradually blends the end of one clip into the beginning of the next.") } },
        { QStringLiteral("composite"),
          { QT_TR_NOOP("Composite"),
            QT_TR_NOOP("Layers one video track on top of another with transparency.") } },
        { QStringLiteral("mix"),
          { QT_TR_NOOP("Audio Mix"),
            QT_TR_NOOP("Blends the audio from two tracks together.") } },
        { QStringLiteral("matte"),
          { QT_TR_NOOP("Matte Transition"),
            QT_TR_NOOP("Uses a grayscale image to define the transition pattern between clips.") } },
    };
    return map;
}

// ---------------------------------------------------------------------------
// EffectCatalog
// ---------------------------------------------------------------------------
EffectCatalog::EffectCatalog(MltEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
{
}

void EffectCatalog::refresh()
{
    m_filters.clear();
    m_transitions.clear();
    m_index.clear();

    if (!m_engine || !m_engine->isInitialized())
        return;

    populateFilters();
    populateTransitions();
}

void EffectCatalog::populateFilters()
{
    auto *repo = m_engine->repository();
    if (!repo) return;

    std::unique_ptr<Mlt::Properties> services(repo->filters());
    if (!services) return;

    for (int i = 0; i < services->count(); ++i) {
        const char *rawName = services->get_name(i);
        if (!rawName) continue;

        const QString serviceId = QString::fromUtf8(rawName);

        // Query metadata
        std::unique_ptr<Mlt::Properties> meta(
            repo->metadata(mlt_service_filter_type, rawName));

        // Skip hidden services
        if (meta && meta->is_valid()) {
            const char *tags = meta->get("tags");
            if (tags && QString::fromUtf8(tags).contains(QStringLiteral("Hidden"),
                                                          Qt::CaseInsensitive))
                continue;
        }

        CatalogEntry entry;
        entry.serviceId = serviceId;
        entry.type = tr("filter");

        // Use curated name if available, otherwise fall back to MLT metadata
        entry.displayName = curatedDisplayName(serviceId);
        entry.description = curatedDescription(serviceId);

        if (entry.displayName.isEmpty() && meta && meta->is_valid()) {
            const char *title = meta->get("title");
            entry.displayName = title ? QString::fromUtf8(title) : serviceId;
        }
        if (entry.description.isEmpty() && meta && meta->is_valid()) {
            const char *desc = meta->get("description");
            if (desc) entry.description = QString::fromUtf8(desc);
        }

        // Categorise (simple heuristic)
        if (serviceId.contains(QStringLiteral("volume"), Qt::CaseInsensitive)
            || serviceId.contains(QStringLiteral("pan"), Qt::CaseInsensitive)
            || serviceId.contains(QStringLiteral("sox"), Qt::CaseInsensitive)
            || serviceId.contains(QStringLiteral("audio"), Qt::CaseInsensitive)
            || serviceId.contains(QStringLiteral("fade"), Qt::CaseInsensitive))
        {
            entry.category = tr("Audio");
        } else {
            entry.category = tr("Video");
        }

        if (entry.displayName.isEmpty())
            entry.displayName = serviceId;

        m_filters.append(entry);
        m_index.insert(serviceId, entry);
    }
}

void EffectCatalog::populateTransitions()
{
    auto *repo = m_engine->repository();
    if (!repo) return;

    std::unique_ptr<Mlt::Properties> services(repo->transitions());
    if (!services) return;

    for (int i = 0; i < services->count(); ++i) {
        const char *rawName = services->get_name(i);
        if (!rawName) continue;

        const QString serviceId = QString::fromUtf8(rawName);

        std::unique_ptr<Mlt::Properties> meta(
            repo->metadata(mlt_service_transition_type, rawName));

        if (meta && meta->is_valid()) {
            const char *tags = meta->get("tags");
            if (tags && QString::fromUtf8(tags).contains(QStringLiteral("Hidden"),
                                                          Qt::CaseInsensitive))
                continue;
        }

        CatalogEntry entry;
        entry.serviceId = serviceId;
        entry.type = tr("transition");

        entry.displayName = curatedDisplayName(serviceId);
        entry.description = curatedDescription(serviceId);

        if (entry.displayName.isEmpty() && meta && meta->is_valid()) {
            const char *title = meta->get("title");
            entry.displayName = title ? QString::fromUtf8(title) : serviceId;
        }
        if (entry.description.isEmpty() && meta && meta->is_valid()) {
            const char *desc = meta->get("description");
            if (desc) entry.description = QString::fromUtf8(desc);
        }

        entry.category = tr("Transition");
        if (entry.displayName.isEmpty())
            entry.displayName = serviceId;

        m_transitions.append(entry);
        m_index.insert(serviceId, entry);
    }
}

QVector<CatalogEntry> EffectCatalog::allEntries() const
{
    QVector<CatalogEntry> all;
    all.reserve(m_filters.size() + m_transitions.size());
    all.append(m_filters);
    all.append(m_transitions);
    return all;
}

QVector<CatalogEntry> EffectCatalog::search(const QString &query) const
{
    QVector<CatalogEntry> results;
    const auto all = allEntries();
    for (const auto &entry : all) {
        if (entry.displayName.contains(query, Qt::CaseInsensitive)
            || entry.description.contains(query, Qt::CaseInsensitive)
            || entry.category.contains(query, Qt::CaseInsensitive))
        {
            results.append(entry);
        }
    }
    return results;
}

const CatalogEntry *EffectCatalog::findByServiceId(const QString &id) const
{
    auto it = m_index.constFind(id);
    return (it != m_index.constEnd()) ? &it.value() : nullptr;
}

QString EffectCatalog::curatedDisplayName(const QString &serviceId) const
{
    const auto &map = curatedNames();
    auto it = map.constFind(serviceId);
    if (it != map.constEnd())
        return tr(it.value().first);
    return {};
}

QString EffectCatalog::curatedDescription(const QString &serviceId) const
{
    const auto &map = curatedNames();
    auto it = map.constFind(serviceId);
    if (it != map.constEnd())
        return tr(it.value().second);
    return {};
}

} // namespace Thrive
