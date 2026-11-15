#include "desktop/suffix_modifier_builder.hpp"

#include <filesystem>
#include <sstream>

namespace openmedia::desktop {
namespace {

bool isKnownModifierToken(const std::string& tok) {
    const size_t eq = tok.find('=');
    const std::string k = (eq == std::string::npos) ? tok : tok.substr(0, eq);
    return k == "w" || k == "h" || k == "vs" || k == "as" || k == "pf" || k == "fr" ||
           k == "ar" || k == "sr" || k == "threads" || k == "cs" || k == "cr" ||
           k == "color_range" || k == "color_space" || k == "pixel_format" ||
           k == "frame_rate" || k == "aspect_ratio" || k == "sample_rate" ||
           k == "accurate_duration" || k == "rft" || k == "reset_first_timestamp";
}

} // namespace

std::string SuffixModifierBuilder::stripModifiers(const std::string& modifiedPath) {
    std::string work = modifiedPath;
    if (work.size() > 7U && work.substr(work.size() - 7U) == ".influx") {
        work.resize(work.size() - 7U);
    } else if (work.size() > 10U && work.substr(work.size() - 10U) == ".openmedia") {
        work.resize(work.size() - 10U);
    }
    const size_t slash = work.find_last_of("\\/");
    const std::string dir = (slash == std::string::npos) ? "" : work.substr(0, slash + 1U);
    const std::string file = (slash == std::string::npos) ? work : work.substr(slash + 1U);
    const size_t fileDot = file.find_last_of('.');
    const std::string ext = (fileDot == std::string::npos) ? "" : file.substr(fileDot);
    std::string stem = (fileDot == std::string::npos) ? file : file.substr(0, fileDot);

    while (true) {
        const size_t bang = stem.rfind('!');
        if (bang == std::string::npos) break;
        const std::string tail = stem.substr(bang + 1U);
        if (!isKnownModifierToken(tail)) break;
        stem.resize(bang);
    }
    return dir + stem + ext;
}

std::string SuffixModifierBuilder::buildFilename(const std::string& originalPath, const SuffixModifierOptions& opts) {
    const std::string clean = stripModifiers(originalPath);
    const size_t slash = clean.find_last_of("\\/");
    const std::string dir = (slash == std::string::npos) ? "" : clean.substr(0, slash + 1U);
    const std::string file = (slash == std::string::npos) ? clean : clean.substr(slash + 1U);
    const size_t dot = file.find_last_of('.');
    const std::string stem = (dot == std::string::npos) ? file : file.substr(0, dot);
    const std::string ext = (dot == std::string::npos) ? "" : file.substr(dot);

    std::ostringstream mod;
    if (opts.width > 0) mod << "!w=" << opts.width;
    if (opts.height > 0) mod << "!h=" << opts.height;
    if (opts.videoStreamIndex >= 0) mod << "!vs=" << opts.videoStreamIndex;
    if (opts.audioStreamIndex >= 0) mod << "!as=" << opts.audioStreamIndex;
    if (!opts.pixelFormat.empty()) mod << "!pf=" << opts.pixelFormat;
    if (!opts.frameRate.empty()) mod << "!fr=" << opts.frameRate;
    if (!opts.aspectRatio.empty()) mod << "!ar=" << opts.aspectRatio;
    if (opts.sampleRate > 0) mod << "!sr=" << opts.sampleRate;
    if (opts.threads > 0) mod << "!threads=" << opts.threads;
    if (!opts.colorSpace.empty()) mod << "!cs=" << opts.colorSpace;
    if (!opts.colorRange.empty()) mod << "!cr=" << opts.colorRange;
    if (opts.accurateDuration) mod << "!accurate_duration";
    if (opts.resetFirstTimestamp) mod << "!rft=1";

    std::string result = dir + stem + mod.str() + ext;
    if (opts.appendInfluxVirtualExt) result += ".influx";
    return result;
}

bool SuffixModifierBuilder::renameWithModifiers(const std::string& originalPath,
                                                const SuffixModifierOptions& opts,
                                                std::string& outNewPath) {
    outNewPath = buildFilename(originalPath, opts);
    std::error_code ec;
    if (outNewPath == originalPath || !std::filesystem::exists(originalPath, ec)) {
        return false;
    }
    std::filesystem::rename(originalPath, outNewPath, ec);
    return !ec;
}

bool SuffixModifierBuilder::restoreOriginalFilename(const std::string& modifiedPath, std::string& outRestoredPath) {
    outRestoredPath = stripModifiers(modifiedPath);
    std::error_code ec;
    if (outRestoredPath == modifiedPath || !std::filesystem::exists(modifiedPath, ec)) {
        return false;
    }
    std::filesystem::rename(modifiedPath, outRestoredPath, ec);
    return !ec;
}

} // namespace openmedia::desktop
