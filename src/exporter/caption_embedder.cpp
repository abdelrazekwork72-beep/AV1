#include "exporter/caption_embedder.hpp"
#include <iomanip>
#include <sstream>
namespace openmedia::exporter {
namespace {
std::string formatTimestamp(int64_t ms, char sep) {
    if (ms < 0) ms = 0;
    const int64_t h = ms / 3600000;
    const int64_t m = (ms % 3600000) / 60000;
    const int64_t s = (ms % 60000) / 1000;
    const int64_t rem = ms % 1000;
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << h << ":"
        << std::setw(2) << m << ":" << std::setw(2) << s << sep
        << std::setw(3) << rem;
    return oss.str();
}
} // namespace
std::string CaptionEmbedder::formatCaptions(const std::vector<CaptionCue>& cues, CaptionFormat fmt) {
    std::ostringstream oss;
    if (fmt == CaptionFormat::WebVTT) oss << "WEBVTT\n\n";
    else if (fmt == CaptionFormat::ASS) oss << "[Script Info]\nScriptType: v4.00+\n\n[Events]\n";
    const char sep = (fmt == CaptionFormat::SRT) ? ',' : '.';
    size_t totalBytes = 0;
    for (size_t i = 0; i < cues.size(); ++i) {
        const std::string clampedText = cues[i].text.substr(0, kMaxCueBytes);
        totalBytes += clampedText.size();
        if (totalBytes > kMaxTrackBytes) break;
        if (fmt == CaptionFormat::ASS) {
            oss << "Dialogue: 0," << formatTimestamp(cues[i].startMs, '.') << ","
                << formatTimestamp(cues[i].endMs, '.') << "," << cues[i].language
                << ",,0,0,0,," << clampedText << "\n";
        } else {
            oss << (i + 1) << " [" << cues[i].language << "]\n"
                << formatTimestamp(cues[i].startMs, sep) << " --> "
                << formatTimestamp(cues[i].endMs, sep) << "\n"
                << clampedText << "\n\n";
        }
    }
    return oss.str();
}
} // namespace openmedia::exporter
