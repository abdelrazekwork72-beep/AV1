#include "exporter/match_source_engine.hpp"

namespace openmedia::exporter {

csSDK_int32 MatchSourceEngine::conformToSequence(bool conformRequested,
                                                 const SequenceMatchProfile& sequence,
                                                 SequenceMatchProfile& target) noexcept {
    if (!conformRequested) {
        return kExNoErr;
    }
    if (sequence.width <= 0 || sequence.height <= 0 || sequence.fpsNum <= 0 || sequence.fpsDen <= 0) {
        return kExError;
    }
    target = sequence;
    return kExNoErr;
}

} // namespace openmedia::exporter
