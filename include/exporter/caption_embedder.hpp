#pragma once
/// @file caption_embedder.hpp
/// @brief Timeline closed-caption extractor and SRT/WebVTT/ASS formatter with TM-3.1.2 bounds.
#include "exporter/exporter_types.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace openmedia::exporter {
enum class CaptionFormat { SRT, WebVTT, ASS };
struct CaptionCue {
    int64_t startMs = 0;
    int64_t endMs = 0;
    std::string text;
    std::string language = "eng";
};
class CaptionEmbedder {
public:
    static constexpr size_t kMaxCueBytes = 4096U;
    static constexpr size_t kMaxTrackBytes = 10U * 1024U * 1024U;
    static std::string formatCaptions(const std::vector<CaptionCue>& cues, CaptionFormat fmt);
};
} // namespace openmedia::exporter
