#include "desktop/suffix_modifier_builder.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace openmedia::desktop;

    SuffixModifierOptions opts;
    opts.width = 1920;
    opts.height = 1080;
    opts.videoStreamIndex = 0;
    opts.audioStreamIndex = 1;
    opts.pixelFormat = "yuv420p";
    opts.frameRate = "60";
    opts.aspectRatio = "16:9";
    opts.sampleRate = 48000;
    opts.threads = 8;
    opts.colorSpace = "bt709";
    opts.colorRange = "full";
    opts.accurateDuration = true;
    opts.resetFirstTimestamp = true;
    opts.appendInfluxVirtualExt = true;

    const std::string built = SuffixModifierBuilder::buildFilename("/media/footage.mkv", opts);
    assert(built.find("!w=1920!h=1080") != std::string::npos);
    assert(built.find("!vs=0") != std::string::npos);
    assert(built.find("!as=1") != std::string::npos);
    assert(built.find("!pf=yuv420p") != std::string::npos);
    assert(built.find("!cr=full") != std::string::npos);
    assert(built.find("!accurate_duration") != std::string::npos);
    assert(built.find(".influx") != std::string::npos);

    const std::string stripped = SuffixModifierBuilder::stripModifiers(built);
    assert(stripped == "/media/footage.mkv");

    std::cout << "test_suffix_modifier_builder: PASSED\n";
    return 0;
}
