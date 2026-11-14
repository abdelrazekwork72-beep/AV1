#include "importer/audio_resampler.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

using namespace openmedia::importer;

int main() {
    std::vector<float> in441(44100, 0.0f);
    for (size_t i = 0; i < in441.size(); ++i) {
        in441[i] = std::sin(2.0 * 3.14159265358979323846 * 440.0 * static_cast<double>(i) / 44100.0);
    }
    std::vector<float> out48;
    assert(AudioResampler::resample(in441, 44100, 48000, out48));
    assert(out48.size() == 48000);

    std::cout << "[PASS] TC-2.4.5 AudioResampler 44.1kHz->48kHz exact 48000 samples passed.\n";
    return 0;
}
