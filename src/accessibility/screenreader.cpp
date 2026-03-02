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

    // Prism v0.7.1 API: prism_init takes a PrismConfig pointer,
    // returns a PrismContext pointer (nullptr on failure).
    PrismConfig cfg = prism_config_init();
    auto *ctx = prism_init(&cfg);
    if (!ctx)
        return false;

    // Auto-detect the best available screen reader backend
    PrismBackend *backend = prism_registry_create_best(ctx);
    if (!backend) {
        prism_shutdown(ctx);
        return false;
    }

    // Initialize the backend
    if (prism_backend_initialize(backend) != PRISM_OK) {
        prism_backend_free(backend);
        prism_shutdown(ctx);
        return false;
    }

    m_context = static_cast<void *>(ctx);
    m_backend = static_cast<void *>(backend);
    m_initialized = true;
    return true;
}

void ScreenReader::shutdown()
{
    if (!m_initialized)
        return;

    if (m_backend) {
        prism_backend_free(static_cast<PrismBackend *>(m_backend));
        m_backend = nullptr;
    }

    if (m_context) {
        prism_shutdown(static_cast<PrismContext *>(m_context));
        m_context = nullptr;
    }

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
        interrupt);
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
        interrupt);
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

    // prism_backend_get_features returns uint64_t directly in v0.7.1
    const uint64_t features =
        prism_backend_get_features(static_cast<PrismBackend *>(m_backend));
    return (features & PRISM_BACKEND_SUPPORTS_SPEAK) != 0;
}

QString ScreenReader::detectedScreenReader() const
{
    if (!m_initialized || !m_backend)
        return tr("None");

    // Prism v0.7.1 exposes backend name
    const char *name =
        prism_backend_name(static_cast<PrismBackend *>(m_backend));
    if (name && *name)
        return QString::fromUtf8(name);
    return tr("Active");
}

} // namespace Thrive
