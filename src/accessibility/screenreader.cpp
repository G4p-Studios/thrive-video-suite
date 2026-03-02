// SPDX-License-Identifier: MIT
// Thrive Video Suite – Screen reader implementation

#include "screenreader.h"

#include <prism.h>

namespace Thrive {

ScreenReader &ScreenReader::instance()
{
    static ScreenReader s_instance;
    return s_instance;
}

ScreenReader::ScreenReader()
    : QObject(nullptr)
{
}

ScreenReader::~ScreenReader()
{
    shutdown();
}

bool ScreenReader::initialize()
{
    if (m_initialized)
        return true;

    if (prism_init() != PRISM_OK)
        return false;

    // Auto-detect the best available screen reader backend
    PrismBackend *backend = nullptr;
    if (prism_registry_create_best(&backend) != PRISM_OK || !backend) {
        prism_shutdown();
        return false;
    }

    m_backend = static_cast<void *>(backend);
    m_initialized = true;
    return true;
}

void ScreenReader::shutdown()
{
    if (!m_initialized)
        return;

    if (m_backend) {
        prism_backend_destroy(static_cast<PrismBackend *>(m_backend));
        m_backend = nullptr;
    }

    prism_shutdown();
    m_initialized = false;
}

void ScreenReader::speak(const QString &text, bool interrupt)
{
    if (!m_initialized || !m_backend)
        return;

    const auto utf8 = text.toUtf8();
    prism_backend_speak(
        static_cast<PrismBackend *>(m_backend),
        utf8.constData(),
        interrupt ? 1 : 0);
}

void ScreenReader::braille(const QString &text)
{
    if (!m_initialized || !m_backend)
        return;

    const auto utf8 = text.toUtf8();
    prism_backend_braille(
        static_cast<PrismBackend *>(m_backend),
        utf8.constData());
}

void ScreenReader::output(const QString &text, bool interrupt)
{
    if (!m_initialized || !m_backend)
        return;

    const auto utf8 = text.toUtf8();
    prism_backend_output(
        static_cast<PrismBackend *>(m_backend),
        utf8.constData(),
        interrupt ? 1 : 0);
}

void ScreenReader::silence()
{
    if (!m_initialized || !m_backend)
        return;

    prism_backend_stop(static_cast<PrismBackend *>(m_backend));
}

bool ScreenReader::isScreenReaderActive() const
{
    if (!m_initialized || !m_backend)
        return false;

    // Check if the backend supports speech — if so, a screen reader is active
    uint64_t features = 0;
    if (prism_backend_get_features(
            static_cast<PrismBackend *>(m_backend), &features) == PRISM_OK)
    {
        return (features & PRISM_BACKEND_SUPPORTS_SPEAK) != 0;
    }
    return false;
}

QString ScreenReader::detectedScreenReader() const
{
    // Prism doesn't expose a "name" directly — we rely on the backend type
    // For now, return a generic indicator
    if (!m_initialized)
        return tr("None");
    return tr("Active");
}

} // namespace Thrive
