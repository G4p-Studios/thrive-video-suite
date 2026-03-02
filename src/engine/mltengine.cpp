// SPDX-License-Identifier: MIT
// Thrive Video Suite – MLT engine implementation

#include "mltengine.h"

#include <mlt++/MltFactory.h>
#include <mlt++/MltProfile.h>
#include <mlt++/MltRepository.h>
#include <framework/mlt_factory.h>

#include <QCoreApplication>
#include <QDir>
#include <algorithm>
#include <numeric>

namespace Thrive {

MltEngine::MltEngine(QObject *parent)
    : QObject(parent)
{
}

MltEngine::~MltEngine()
{
    shutdown();
}

bool MltEngine::initialize()
{
    if (m_initialized)
        return true;

    // Let MLT auto-detect its module directory from the executable location
    m_repository = Mlt::Factory::init(nullptr);
    if (!m_repository)
        return false;

    // Default composition: 1080p @ 25fps
    m_compositionProfile = std::make_unique<Mlt::Profile>();
    m_compositionProfile->set_width(1920);
    m_compositionProfile->set_height(1080);
    m_compositionProfile->set_frame_rate(25, 1);
    m_compositionProfile->set_progressive(1);
    m_compositionProfile->set_sample_aspect(1, 1);
    m_compositionProfile->set_display_aspect(16, 9);
    m_compositionProfile->set_explicit(1);

    // Build matching preview profile at 640p
    m_previewProfile = std::make_unique<Mlt::Profile>();
    rebuildPreviewProfile();

    m_initialized = true;
    return true;
}

void MltEngine::shutdown()
{
    if (!m_initialized)
        return;

    m_previewProfile.reset();
    m_compositionProfile.reset();
    Mlt::Factory::close();
    m_repository = nullptr;
    m_initialized = false;
}

void MltEngine::setCompositionProfile(int width, int height, double fps)
{
    if (!m_compositionProfile) return;

    m_compositionProfile->set_width(width);
    m_compositionProfile->set_height(height);

    // Decompose fps into numerator/denominator (handles 29.97 etc.)
    if (fps == 29.97) {
        m_compositionProfile->set_frame_rate(30000, 1001);
    } else if (fps == 23.976) {
        m_compositionProfile->set_frame_rate(24000, 1001);
    } else {
        m_compositionProfile->set_frame_rate(static_cast<int>(fps), 1);
    }

    // Auto-detect aspect ratio
    const int gcd = std::gcd(width, height);
    m_compositionProfile->set_display_aspect(width / gcd, height / gcd);
    m_compositionProfile->set_explicit(1);

    rebuildPreviewProfile();
}

void MltEngine::setPreviewScale(int height)
{
    if (height != m_previewHeight) {
        m_previewHeight = height;
        rebuildPreviewProfile();
        emit previewScaleChanged(m_previewHeight);
    }
}

void MltEngine::rebuildPreviewProfile()
{
    if (!m_compositionProfile || !m_previewProfile) return;

    // Copy temporal and colour properties from composition
    m_previewProfile->set_frame_rate(
        m_compositionProfile->frame_rate_num(),
        m_compositionProfile->frame_rate_den());
    m_previewProfile->set_progressive(m_compositionProfile->progressive());
    m_previewProfile->set_sample_aspect(
        m_compositionProfile->sample_aspect_num(),
        m_compositionProfile->sample_aspect_den());
    m_previewProfile->set_display_aspect(
        m_compositionProfile->display_aspect_num(),
        m_compositionProfile->display_aspect_den());
    m_previewProfile->set_colorspace(m_compositionProfile->colorspace());

    // Compute preview dimensions preserving aspect ratio
    const int compW = m_compositionProfile->width();
    const int compH = m_compositionProfile->height();
    const int previewH = std::min(m_previewHeight, compH);

    int previewW = static_cast<int>(
        static_cast<double>(previewH) * compW / compH);
    // Coerce to even number (required for YUV subsampling)
    previewW += previewW % 2;

    m_previewProfile->set_width(previewW);
    m_previewProfile->set_height(previewH);
    m_previewProfile->set_explicit(1);
}

QString MltEngine::modulesPath() const
{
    return QString::fromUtf8(mlt_factory_directory());
}

} // namespace Thrive
