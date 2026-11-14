#include "exporter/rate_control_normalizer.hpp"
#include <algorithm>
#include <cmath>
namespace openmedia::exporter {
int32_t RateControlNormalizer::toAv1BaseQIndex(int32_t uiQuality) noexcept {
    const int32_t q = std::clamp(uiQuality, 0, 63);
    const auto mapped = static_cast<int32_t>(std::lround(static_cast<double>(q) * 255.0 / 63.0));
    return std::clamp(mapped, 0, 255);
}
int32_t RateControlNormalizer::toNvencCq(int32_t uiQuality) noexcept {
    const int32_t q = std::clamp(uiQuality, 0, 63);
    const auto mapped = static_cast<int32_t>(std::lround(static_cast<double>(q) * 51.0 / 63.0));
    return std::clamp(mapped, 0, 51);
}
int32_t RateControlNormalizer::toVp9Crf(int32_t uiQuality) noexcept {
    return std::clamp(uiQuality, 0, 63);
}
} // namespace openmedia::exporter
