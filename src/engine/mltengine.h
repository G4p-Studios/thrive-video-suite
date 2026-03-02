// SPDX-License-Identifier: MIT
// Thrive Video Suite – MLT engine initialisation and profile management

#pragma once

#include <QObject>
#include <QString>
#include <memory>

namespace Mlt {
class Factory;
class Profile;
class Repository;
} // namespace Mlt

namespace Thrive {

/// Manages MLT Framework lifecycle and the dual-profile architecture
/// (composition profile at full resolution, preview profile at reduced resolution).
class MltEngine : public QObject
{
    Q_OBJECT

public:
    explicit MltEngine(QObject *parent = nullptr);
    ~MltEngine() override;

    /// Initialise MLT. Call once at application startup.
    bool initialize();

    /// Shut down MLT. Called automatically on destruction.
    void shutdown();

    [[nodiscard]] bool isInitialized() const { return m_initialized; }

    // Profiles
    [[nodiscard]] Mlt::Profile *compositionProfile() const { return m_compositionProfile.get(); }
    [[nodiscard]] Mlt::Profile *previewProfile()     const { return m_previewProfile.get(); }

    /// Set the composition (full-resolution) profile. E.g. 1920×1080 @ 25fps.
    void setCompositionProfile(int width, int height, double fps);

    /// Change preview resolution. Aspect ratio is computed from the composition profile.
    /// Height must be one of: 360, 640, 720.
    void setPreviewScale(int height);

    [[nodiscard]] int previewHeight() const { return m_previewHeight; }

    /// Access the MLT repository for enumerating services.
    [[nodiscard]] Mlt::Repository *repository() const { return m_repository; }

    /// Path to the MLT modules directory.
    [[nodiscard]] QString modulesPath() const;

signals:
    void previewScaleChanged(int height);

private:
    void rebuildPreviewProfile();

    bool m_initialized = false;
    int  m_previewHeight = 640;

    std::unique_ptr<Mlt::Profile> m_compositionProfile;
    std::unique_ptr<Mlt::Profile> m_previewProfile;
    Mlt::Repository *m_repository = nullptr;   // owned by MLT factory
};

} // namespace Thrive
