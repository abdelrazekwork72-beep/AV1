#include "importer/audio_extractor.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

using namespace openmedia::importer;

int main() {
    std::vector<int16_t> pcm = {16384, -16384, 32767, -32767, -32768, 0};
    std::vector<float> ch0(5, 99.0f);
    std::vector<float> ch1(5, 99.0f);
    float* planes[2] = {ch0.data(), ch1.data()};

    ImImportAudioRec7Ext rec{};
    rec.position = 0;
    rec.size = 5;
    rec.buffer = planes;

    assert(AudioExtractor::importAudio7(&rec, pcm, 2) == kImNoErr);
    assert(std::fabs(ch0[1] - 1.0f) < 1e-6f);
    assert(std::fabs(ch1[1] + 1.0f) < 1e-6f);
    assert(std::fabs(ch0[2] + 1.0f) < 1e-6f);
    assert(ch0[3] == 0.0f && ch0[4] == 0.0f);
    assert(ch1[3] == 0.0f && ch1[4] == 0.0f);

    std::cout << "[PASS] TC-2.4.1 AudioExtractor 32767.0 divisor, 65536 cap & EOF silence zero-fill passed.\n";
    return 0;
}
