#include "importer/audio_extractor.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

using namespace openmedia::importer;

int main() {
    std::vector<int16_t> pcm = {16384, -16384, 32767, -32768};
    std::vector<float> ch0(2, 0.0f);
    std::vector<float> ch1(2, 0.0f);
    float* planes[2] = {ch0.data(), ch1.data()};

    ImImportAudioRec7Ext rec{};
    rec.position = 0;
    rec.size = 2;
    rec.buffer = planes;

    assert(AudioExtractor::importAudio7(&rec, pcm, 2) == kImNoErr);
    assert(std::fabs(ch0[0] - 0.5f) < 1e-4f);
    assert(std::fabs(ch1[0] + 0.5f) < 1e-4f);
    assert(std::fabs(ch1[1] + 1.0f) < 1e-4f);

    std::cout << "[PASS] TC-2.4.1 AudioExtractor 32-bit planar float imImportAudio7 passed.\n";
    return 0;
}
