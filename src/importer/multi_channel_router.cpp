/// @file multi_channel_router.cpp
/// @brief Implementation of MultiChannelRouter supporting 5/6-channel MOV PCM and up to 24/32 channels.

#include "importer/multi_channel_router.hpp"

#include <array>

namespace openmedia::importer {
namespace {

constexpr std::array<PrAudioChannelLabelId, 5> kSurround5 = {
    PrAudioChannelLabelId::Left,
    PrAudioChannelLabelId::Right,
    PrAudioChannelLabelId::Center,
    PrAudioChannelLabelId::LeftSurround,
    PrAudioChannelLabelId::RightSurround,
};

constexpr std::array<PrAudioChannelLabelId, 6> kSurround6 = {
    PrAudioChannelLabelId::Left,
    PrAudioChannelLabelId::Right,
    PrAudioChannelLabelId::Center,
    PrAudioChannelLabelId::LFE,
    PrAudioChannelLabelId::LeftSurround,
    PrAudioChannelLabelId::RightSurround,
};

constexpr std::array<PrAudioChannelLabelId, 8> kSurround8 = {
    PrAudioChannelLabelId::Left,
    PrAudioChannelLabelId::Right,
    PrAudioChannelLabelId::Center,
    PrAudioChannelLabelId::LFE,
    PrAudioChannelLabelId::LeftSurround,
    PrAudioChannelLabelId::RightSurround,
    PrAudioChannelLabelId::LeftBack,
    PrAudioChannelLabelId::RightBack,
};

} // namespace

csSDK_int32 MultiChannelRouter::resolveLayout(uint32_t numChannels,
                                              ImAudioChannelLayoutRecExt& outLayout) {
    if (numChannels == 0 || numChannels > kMaxSupportedChannels) {
        return kImMemErr;
    }
    outLayout.numChannels = static_cast<csSDK_int32>(numChannels);
    outLayout.channelLabels.clear();
    outLayout.channelLabels.reserve(numChannels);

    if (numChannels == 2U) {
        outLayout.channelLabels = {PrAudioChannelLabelId::Left, PrAudioChannelLabelId::Right};
        return kImNoErr;
    }
    if (numChannels == 5U) {
        outLayout.channelLabels.assign(kSurround5.begin(), kSurround5.end());
        return kImNoErr;
    }
    if (numChannels == 6U) {
        outLayout.channelLabels.assign(kSurround6.begin(), kSurround6.end());
        return kImNoErr;
    }
    if (numChannels == 8U) {
        outLayout.channelLabels.assign(kSurround8.begin(), kSurround8.end());
        return kImNoErr;
    }
    for (uint32_t i = 0; i < numChannels; ++i) {
        outLayout.channelLabels.push_back(static_cast<PrAudioChannelLabelId>(
            static_cast<uint32_t>(PrAudioChannelLabelId::DiscreteBase) + i));
    }
    return kImNoErr;
}

} // namespace openmedia::importer
