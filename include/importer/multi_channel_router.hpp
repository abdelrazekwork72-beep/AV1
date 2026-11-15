#pragma once
/// @file multi_channel_router.hpp
/// @brief Multi-channel audio layout mapper for imGetAudioChannelLayout (1..32 channels, 5/6-ch MOV PCM).

#include "importer/importer_types.hpp"

#include <cstdint>

namespace openmedia::importer {

/// @brief Maps channel counts (Mono, Stereo, 5.0, 5.1, 7.1, 24/32 discrete) to PrAudioChannelLabelId.
class MultiChannelRouter {
public:
    static constexpr uint32_t kMaxSupportedChannels = 32U;

    /// @brief Populate ImAudioChannelLayoutRecExt for the given channel count (1..32).
    static csSDK_int32 resolveLayout(uint32_t numChannels, ImAudioChannelLayoutRecExt& outLayout);
};

} // namespace openmedia::importer
