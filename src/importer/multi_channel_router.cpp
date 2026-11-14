#include "importer/multi_channel_router.hpp"

namespace openmedia::importer {

csSDK_int32 MultiChannelRouter::resolveLayout(uint32_t numChannels,
                                              ImAudioChannelLayoutRecExt& outLayout) {
    if (numChannels == 0 || numChannels > 32U) {
        return kImMemErr;
    }
    outLayout.numChannels = numChannels;
    outLayout.channelLabels.clear();
    outLayout.channelLabels.reserve(numChannels);

    static constexpr PrAudioChannelLabelId kSurround6[] = {
        PrAudioChannelLabelId::Left,
        PrAudioChannelLabelId::Right,
        PrAudioChannelLabelId::Center,
        PrAudioChannelLabelId::LFE,
        PrAudioChannelLabelId::LeftSurround,
        PrAudioChannelLabelId::RightSurround,
    };

    if (numChannels == 2U) {
        outLayout.channelLabels.push_back(PrAudioChannelLabelId::Left);
        outLayout.channelLabels.push_back(PrAudioChannelLabelId::Right);
        return kImNoErr;
    }
    if (numChannels == 6U) {
        for (const auto lbl : kSurround6) {
            outLayout.channelLabels.push_back(lbl);
        }
        return kImNoErr;
    }
    for (uint32_t i = 0; i < numChannels; ++i) {
        outLayout.channelLabels.push_back(static_cast<PrAudioChannelLabelId>(
            static_cast<uint32_t>(PrAudioChannelLabelId::DiscreteBase) + i));
    }
    return kImNoErr;
}

} // namespace openmedia::importer
