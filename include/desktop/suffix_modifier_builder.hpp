#pragma once
/// @file suffix_modifier_builder.hpp
/// @brief Interactive !options.influx / .openmedia 13-token filename modifier builder.

#include "desktop/desktop_types.hpp"

#include <string>

namespace openmedia::desktop {

struct SuffixModifierOptions {
    int32_t width = 0;
    int32_t height = 0;
    int32_t videoStreamIndex = -1;
    int32_t audioStreamIndex = -1;
    std::string pixelFormat;
    std::string frameRate;
    std::string aspectRatio;
    int32_t sampleRate = 0;
    int32_t threads = 0;
    std::string colorSpace;
    std::string colorRange; ///< "full" or "limited"
    bool accurateDuration = false;
    bool resetFirstTimestamp = false;
    bool appendInfluxVirtualExt = true;
};

class SuffixModifierBuilder {
public:
    static std::string buildFilename(const std::string& originalPath, const SuffixModifierOptions& opts);
    static std::string stripModifiers(const std::string& modifiedPath);
    static bool renameWithModifiers(const std::string& originalPath,
                                    const SuffixModifierOptions& opts,
                                    std::string& outNewPath);
    static bool restoreOriginalFilename(const std::string& modifiedPath, std::string& outRestoredPath);
};

} // namespace openmedia::desktop
