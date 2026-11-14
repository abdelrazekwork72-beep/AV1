#include "importer/multi_channel_router.hpp"

#include <cassert>
#include <iostream>

using namespace openmedia::importer;

int main() {
    ImAudioChannelLayoutRecExt layout{};
    assert(MultiChannelRouter::resolveLayout(2, layout) == kImNoErr);
    assert(layout.channelLabels.size() == 2);
    assert(layout.channelLabels[0] == PrAudioChannelLabelId::Left);

    assert(MultiChannelRouter::resolveLayout(6, layout) == kImNoErr);
    assert(layout.channelLabels[2] == PrAudioChannelLabelId::Center);
    assert(layout.channelLabels[3] == PrAudioChannelLabelId::LFE);

    assert(MultiChannelRouter::resolveLayout(32, layout) == kImNoErr);
    assert(layout.channelLabels.size() == 32);
    assert(MultiChannelRouter::resolveLayout(33, layout) == kImMemErr);

    std::cout << "[PASS] TC-2.4.4 MultiChannelRouter 2ch/5.1/32ch mapping passed.\n";
    return 0;
}
