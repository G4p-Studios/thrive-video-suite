// SPDX-License-Identifier: MIT
// Thrive Video Suite – MLT engine implementation

#include "mltengine.h"

#include <mlt++/MltFactory.h>
#include <mlt++/MltProfile.h>
#include <mlt++/MltRepository.h>
#include <mlt++/MltConsumer.h>
#include <mlt++/MltProducer.h>
#include <framework/mlt_factory.h>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

#include <QCoreApplication>
#include <QDir>
#include <QDebug>
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

    // Compute paths relative to the executable so MLT can find its
    // plugin modules (lib/mlt-7/) and data files (share/mlt-7/).
    const QDir appDir(QCoreApplication::applicationDirPath());
    const QString repoPath = appDir.absoluteFilePath(QStringLiteral("lib/mlt-7"));
    const QString dataPath = appDir.absoluteFilePath(QStringLiteral("share/mlt-7"));
    const QString profilesPath = dataPath + QStringLiteral("/profiles");

    qputenv("MLT_REPOSITORY", repoPath.toUtf8());
    qputenv("MLT_DATA",       dataPath.toUtf8());
    qputenv("MLT_PROFILES_PATH", profilesPath.toUtf8());

    // Tell MLT to skip modules with external dependencies we don't ship.
    // These cause "Entry Point Not Found" dialogs on Windows because they
    // were compiled against different library versions (MinGW libstdc++,
    // different Qt, OpenGL, JACK, OpenCV, etc.).
    // Colon-separated list of module basenames (without .dll).
    qputenv("MLT_REPOSITORY_DENY",
            "libmltglaxnimate-qt6:libmltqt6:"        // wrong Qt version
            "libmltrtaudio:libmltsox:libmltrubberband:" // missing libstdc++
            "libmltdecklink:libmltjackrack:"          // hardware/JACK deps
            "libmltfrei0r:libmltladspa:"              // plugin host deps
            "libmltmovit:libmltopencv:"               // OpenGL / OpenCV
            "libmltoldfilm:libmltkdenlive:"            // non-essential
            "libmltspatialaudio:libmltvidstab:"        // non-essential
            "libmltxine");                             // non-essential

    // Ensure the exe directory (which has MinGW runtime DLLs like
    // libgcc_s_seh-1.dll) is on PATH so loaded modules can find them.
    const QByteArray exeDir = appDir.absolutePath().toUtf8();
    QByteArray currentPath = qgetenv("PATH");
    if (!currentPath.contains(exeDir)) {
        qputenv("PATH", exeDir + ";" + currentPath);
    }

    qDebug() << "MltEngine: MLT_REPOSITORY =" << repoPath;
    qDebug() << "MltEngine: MLT_DATA       =" << dataPath;
    qDebug() << "MltEngine: repo dir exists?" << QDir(repoPath).exists();
    qDebug() << "MltEngine: data dir exists?" << QDir(dataPath).exists();

    // List module files found in the repository directory
    {
        QDir moduleDir(repoPath);
        const auto entries = moduleDir.entryList(QStringList{QStringLiteral("*.dll")},
                                                  QDir::Files);
        qDebug() << "MltEngine: module DLLs found:" << entries.size();
        for (const auto &e : entries)
            qDebug() << "  " << e;
    }

#ifdef Q_OS_WIN
    // Suppress Windows "Entry Point Not Found" / missing-DLL error
    // dialogs that appear when MLT tries to load modules compiled against
    // different library versions (e.g. libmltglaxnimate-qt6, libmltmovit).
    // The modules simply fail to load silently instead.
    const UINT oldErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);

    // Also add the exe directory to the DLL search path so that modules
    // in lib/mlt-7 can find MinGW runtime DLLs (libgcc, libwinpthread)
    // that live next to the executable.
    AddDllDirectory(reinterpret_cast<PCWSTR>(appDir.absolutePath()
                        .replace(QLatin1Char('/'), QLatin1Char('\\'))
                        .utf16()));
#endif

    // Pass the path DIRECTLY to Factory::init — passing nullptr causes MLT
    // to use a compiled-in default which resolves to "lib/mlt" not "lib/mlt-7".
    m_repository = Mlt::Factory::init(repoPath.toUtf8().constData());

#ifdef Q_OS_WIN
    SetErrorMode(oldErrorMode);
#endif
    if (!m_repository) {
        qWarning() << "MltEngine: Factory::init returned null!";
        return false;
    }

    qDebug() << "MltEngine: Factory::init OK, factory_directory ="
             << mlt_factory_directory();

    // Check if key services are available
    {
        auto *profile = new Mlt::Profile();
        Mlt::Consumer testSdl(*profile, "sdl2_audio");
        qDebug() << "MltEngine: sdl2_audio consumer valid?" << testSdl.is_valid();
        Mlt::Consumer testRtaudio(*profile, "rtaudio");
        qDebug() << "MltEngine: rtaudio consumer valid?" << testRtaudio.is_valid();
        Mlt::Producer testColor(*profile, "color:red");
        qDebug() << "MltEngine: color producer valid?" << testColor.is_valid();
        delete profile;
    }

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
