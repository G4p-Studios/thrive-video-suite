// SPDX-License-Identifier: MIT
// Thrive Video Suite – Application-wide constants

#pragma once

namespace Thrive {

/// Application metadata
inline constexpr const char *kAppName        = "Thrive Video Suite";
inline constexpr const char *kOrgName        = "ThriveProject";
inline constexpr const char *kOrgDomain      = "thrivevideo.org";
inline constexpr const char *kAppVersion     = "0.1.0";

/// Project file extension (ZIP container)
inline constexpr const char *kProjectExtension = ".tvs";

/// Exit code returned when the app wants to restart itself (e.g.
/// after installing a plugin).
inline constexpr int EXIT_RESTART = 42;

/// Default preview height in pixels (640p).
inline constexpr int kDefaultPreviewScale = 640;

/// Default frames per second for new projects.
inline constexpr double kDefaultFps = 30.0;

/// Default composition resolution.
inline constexpr int kDefaultWidth  = 1920;
inline constexpr int kDefaultHeight = 1080;

/// QSettings keys
inline constexpr const char *kSettingsShortcuts      = "shortcuts";
inline constexpr const char *kSettingsPluginJustInstalled
    = "plugins/justInstalled";
inline constexpr const char *kSettingsPreviewScale   = "preview/scale";
inline constexpr const char *kSettingsScrubAudio     = "preview/scrubAudio";
inline constexpr const char *kSettingsAudioCuesOn    = "accessibility/audioCues";
inline constexpr const char *kSettingsAudioCueVolume = "accessibility/audioCueVolume";
inline constexpr const char *kSettingsFirstRun       = "app/firstRun";

} // namespace Thrive
