#pragma once
/// @file match_source_engine.hpp
/// @brief Synchronizes exporter output settings with Premiere sequence parameters (Match Source).

#include "exporter/exporter_types.hpp"

#include <cstdint>

namespace openmedia::exporter {

/// @brief Sequence properties queried during doConformToMatchParams.
struct SequenceMatchProfile {
    int32_t width = 1920;
    int32_t height = 1080;
    int32_t fpsNum = 30000;
    int32_t fpsDen = 1001;
    int32_t fieldOrder = 0;
    double pixelAspectRatio = 1.0;
};

/// @brief Handles canMatchSource and doConformToMatchParams synchronization.
class MatchSourceEngine {
public:
    /// @brief Conform target profile to source sequence when conformRequested is true.
    static csSDK_int32 conformToSequence(bool conformRequested,
                                         const SequenceMatchProfile& sequence,
                                         SequenceMatchProfile& target) noexcept;
};

} // namespace openmedia::exporter
