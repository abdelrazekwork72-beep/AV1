#pragma once
/// @file file_probe.hpp
/// @brief File probing, OpenMediaConfig (Config.toml), FilenameModifiers (!), and ErrorDeduplicator.

#include "PrSDKPixelFormat.h"
#include "importer/importer_types.hpp"

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace openmedia::importer {

/// @brief GPU decode policy mode configured in Config.toml.
enum class GpuDecodeMode : uint8_t {
    Recommended = 0,
    Available,
    Disabled
};

/// @brief Thumbnail timecode display behavior configured in Config.toml.
enum class ThumbnailTimecodeMode : uint8_t {
    AlwaysShow = 0,
    HideWhenZero,
    AlwaysHide
};

/// @brief Duration calculation mode configured in Config.toml.
enum class DurationCalculationMode : uint8_t {
    Metadata = 0,
    FrameTimestamp
};

/// @brief Parsed Config.toml settings for OpenMedia Importer.
struct OpenMediaConfig {
    bool disable = false;
    bool overrideAdobe = false;
    std::vector<std::string> overrideExtensions;
    bool disableAutomaticOverride = false;
    bool reduceWarnings = false;
    GpuDecodeMode gpuDecodeMode = GpuDecodeMode::Recommended;
    uint32_t maxFrameCacheSizeMB = 400;
    uint32_t maxPercentUsageAvailableMemory = 50;
    bool disableSeekingTable = false;
    bool disableThumbnailCache = false;
    ThumbnailTimecodeMode thumbnailTimecode = ThumbnailTimecodeMode::AlwaysShow;
    DurationCalculationMode durationCalculation = DurationCalculationMode::Metadata;
    std::string uiLanguage = "en";
    std::string importPolicy = "recommended_fallback";
    bool debugLogs = false;
    bool disableMultithreading = false;
    bool disableFrameCache = false;
    int32_t maxLogFileSizeMB = 25;

    /// @brief Parses Config.toml content into an OpenMediaConfig struct.
    static OpenMediaConfig parseConfigToml(const std::string& tomlContent);

    /// @brief Resolves the default platform Config.toml path (%APPDATA%/OpenMedia/Config.toml or ~/.config/OpenMedia/Config.toml).
    static std::string resolveDefaultConfigPath();

    /// @brief Loads Config.toml from the default platform path, returning defaults if absent.
    static OpenMediaConfig loadDefaultConfig();

    /// @brief Checks whether an extension (e.g. ".mkv" or "MKV") is in overrideExtensions (case-insensitive).
    bool isExtensionOverridden(const std::string& ext) const;
};

/// @brief Parsed per-file overrides after the last '!' before the extension.
struct FilenameModifiers {
    int32_t width = 0;
    int32_t height = 0;
    int32_t videoStreamIndex = -1;
    int32_t audioStreamIndex = -1;
    std::string pixelFormat;
    openmedia::RationalTime frameRate{0, 1};
    openmedia::ColorRange colorRange = openmedia::ColorRange::Unspecified;
    bool accurateDuration = false;
    bool resetFirstTimestamp = false;
    std::string aspectRatio;
    int32_t sampleRate = 0;
    int32_t threads = 0;
    std::string colorSpace;
    std::string cleanPath;

    /// @brief Parses filename suffix modifiers after the last '!' before the extension (including .influx / .openmedia).
    static FilenameModifiers parseFilenameModifiers(const std::string& path);

    /// @brief Resolves missing width or height while preserving source aspect ratio.
    static void resolveAspectRatio(int32_t srcWidth,
                                   int32_t srcHeight,
                                   int32_t& ioWidth,
                                   int32_t& ioHeight) noexcept;
};

/// @brief Suppresses repeated per-frame error/warning logs for the same file instance.
class ErrorDeduplicator {
public:
    /// @brief Returns true if the warning should be logged; suppresses duplicates when reduceWarnings is true.
    bool shouldLogWarning(const std::string& fileInstanceKey,
                          const std::string& warningKey,
                          bool reduceWarnings);

    /// @brief Clears tracked warning state for a closed file instance.
    void clearFile(const std::string& fileInstanceKey);

    /// @brief Clears all tracked warnings.
    void clearAll();

private:
    std::mutex mutex_;
    std::unordered_set<std::string> seenWarnings_;
};

/// @brief Handles imOpenFile8 probing, native format delegation, and imGetInfo8 population.
class FileProbe {
public:
    static std::string utf16ToUtf8(const std::u16string& utf16);
    static bool shouldDelegateToNative(const std::vector<StreamInfo>& streams, const std::string& ext);
    static csSDK_int32 openAndProbe(ImOpenFileRec8Ext& openRec, ICoreDemuxer& demuxer);
    static csSDK_int32 populateFileInfo(const std::vector<StreamInfo>& streams, ImFileInfoRec8Ext& infoRec);
    static void applyModifiersToStreams(const FilenameModifiers& mods, std::vector<StreamInfo>& streams);
};

} // namespace openmedia::importer
