#include "exporter/caption_embedder.hpp"

#include <cassert>
#include <iostream>

using namespace openmedia::exporter;

int main() {
    std::vector<CaptionCue> cues = {{1000, 3500, "Hello AV1"}};
    const std::string srt = CaptionEmbedder::formatCaptions(cues, CaptionFormat::SRT);
    const std::string vtt = CaptionEmbedder::formatCaptions(cues, CaptionFormat::WebVTT);
    const std::string ass = CaptionEmbedder::formatCaptions(cues, CaptionFormat::ASS);

    assert(srt.find("00:00:01,000 --> 00:00:03,500") != std::string::npos);
    assert(vtt.find("WEBVTT") == 0);
    assert(ass.find("Dialogue: 0,") != std::string::npos);

    std::cout << "[PASS] TC-3.1.4 CaptionEmbedder SRT/WebVTT/ASS formatting passed.\n";
    return 0;
}
