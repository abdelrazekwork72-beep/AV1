#include "exporter/rate_control_normalizer.hpp"
#include <algorithm>
namespace openmedia::exporter {
int32_t RateControlNormalizer::toAv1BaseQIndex(int32_t uiQuality) noexcept {
    const int32_t q = std::clamp(uiQuality, 0, 63);
    return (q * 255) / 63;
}
int32_t RateControlNormalizer::toNvencCq(int32_t uiQuality) noexcept {
    const int32_t q = std::clamp(uiQuality, 0, 63);
    return (q * 51) / 63;
}
int32_t RateControlNormalizer::toVp9Crf(int32_t uiQuality) noexcept {
    return std::clamp(uiQuality, 0, 63);
}
} // namespace openmedia::exporter
