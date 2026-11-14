#pragma once
/// @file rate_control_normalizer.hpp
/// @brief Translates unified UI quality (0..63) to base_q_index (0..255), cq/qp (0..51), and VP9 crf.
#include <cstdint>
namespace openmedia::exporter {
/// @brief Normalizes rate-control quality scales across AV1, NVENC/HEVC, and VP9.
class RateControlNormalizer {
public:
    static int32_t toAv1BaseQIndex(int32_t uiQuality) noexcept;
    static int32_t toNvencCq(int32_t uiQuality) noexcept;
    static int32_t toVp9Crf(int32_t uiQuality) noexcept;
};
} // namespace openmedia::exporter
