#pragma once
/// @file multi_channel_router.hpp
/// @brief Multi-channel audio layout mapper for imGetAudioChannelLayout (1..32 channels).

#include "importer/importer_types.hpp"

#include <cstdint>

namespace openmedia::importer {

/// @brief Maps channel counts (Mono, Stereo, 5.1, 7.1, up to 32 discrete) to PrAudioChannelLabelId.
class MultiChannelRouter {
public:
    /// @brief Populate ImAudioChannelLayoutRecExt for the given channel count.
    /// @param numChannels Source channel count (1..32).
    /// @param outLayout Output channel layout record.
    /// @return kImNoErr on success, kImMemErr if channel count is invalid.
    static csSDK_int32 resolveLayout(uint32_t numChannels, ImAudioChannelLayoutRecExt& outLayout);
};

} // namespace openmedia::importer
