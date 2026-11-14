#pragma once
/// @file caption_embedder.hpp
/// @brief Timeline closed-caption extractor and SRT/WebVTT/ASS formatter.

#include "exporter/exporter_types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace openmedia::exporter {

/// @brief Subtitle output format.
enum class CaptionFormat { SRT, WebVTT, ASS };

/// @brief Single caption cue from PrSDKCaptioningSuite.
struct CaptionCue {
    int64_t startMs = 0;
    int64_t endMs = 0;
    std::string text;
};

/// @brief Formats closed captions into SRT, WebVTT, or ASS packets for ICoreMuxer.
class CaptionEmbedder {
public:
    /// @brief Format a list of cues into the requested subtitle container format.
    static std::string formatCaptions(const std::vector<CaptionCue>& cues, CaptionFormat fmt);
};

} // namespace openmedia::exporter
