#include "exporter/audio_pull_orchestrator.hpp"

namespace openmedia::exporter {

csSDK_int32 AudioPullOrchestrator::interleavePlanar(const std::vector<std::vector<float>>& planar,
                                                    std::vector<float>& outPacked) {
    if (planar.empty() || planar[0].empty()) {
        return kExError;
    }
    const size_t ch = planar.size();
    const size_t samples = planar[0].size();
    outPacked.resize(ch * samples);
    for (size_t i = 0; i < samples; ++i) {
        for (size_t c = 0; c < ch; ++c) {
            outPacked[i * ch + c] = planar[c][i];
        }
    }
    return kExNoErr;
}

} // namespace openmedia::exporter
