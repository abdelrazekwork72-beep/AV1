/// @file file_probe.cpp
/// @brief Implementation of FileProbe, OpenMediaConfig (Config.toml), FilenameModifiers, and ErrorDeduplicator.

#include "importer/file_probe.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace openmedia::importer {
namespace {

void appendCodePointUtf8(uint32_t cp, std::string& out) {
    if (cp <= 0x7FU) {
        out.push_back(static_cast<char>(cp));
        return;
    }
    if (cp <= 0x7FFU) {
        out.push_back(static_cast<char>(0xC0U | (cp >> 6U)));
        out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
        return;
    }
    if (cp <= 0xFFFFU) {
        out.push_back(static_cast<char>(0xE0U | (cp >> 12U)));
        out.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
        return;
    }
    out.push_back(static_cast<char>(0xF0U | (cp >> 18U)));
    out.push_back(static_cast<char>(0x80U | ((cp >> 12U) & 0x3FU)));
    out.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU)));
    out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
}

bool isHighSurrogate(char16_t unit) noexcept {
    return unit >= 0xD800U && unit <= 0xDBFFU;
}

bool isLowSurrogate(char16_t unit) noexcept {
    return unit >= 0xDC00U && unit <= 0xDFFFU;
}

uint32_t decodeNextCodePoint(const std::u16string& utf16, size_t& index) noexcept {
    const char16_t unit = utf16[index];
    if (isHighSurrogate(unit) && (index + 1U) < utf16.size() && isLowSurrogate(utf16[index + 1U])) {
        const uint32_t hi = static_cast<uint32_t>(unit) - 0xD800U;
        const uint32_t lo = static_cast<uint32_t>(utf16[index + 1U]) - 0xDC00U;
        ++index;
        return 0x10000U + (hi << 10U) + lo;
    }
    if (isHighSurrogate(unit) || isLowSurrogate(unit)) {
        return 0xFFFDU;
    }
    return static_cast<uint32_t>(unit);
}

std::string toLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string trimWhitespace(const std::string& s) {
    const size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const size_t last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1U);
}

std::string stripQuotes(const std::string& s) {
    if (s.size() >= 2U && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
        return s.substr(1U, s.size() - 2U);
    }
    return s;
}

std::string normalizeExtension(const std::string& ext) {
    std::string clean = stripQuotes(trimWhitespace(ext));
    if (!clean.empty() && clean.front() == '.') {
        clean.erase(clean.begin());
    }
    return toLowerAscii(clean);
}

std::string extractExtension(const std::string& path) {
    const size_t slashPos = path.find_last_of("/\\");
    const size_t dotPos = path.find_last_of('.');
    if (dotPos == std::string::npos || (slashPos != std::string::npos && dotPos < slashPos)) {
        return "";
    }
    return normalizeExtension(path.substr(dotPos + 1U));
}

std::string stripTomlComment(const std::string& line) {
    bool inQuote = false;
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '"') {
            inQuote = !inQuote;
        } else if (line[i] == '#' && !inQuote) {
            return line.substr(0, i);
        }
    }
    return line;
}

bool parseBoolToken(const std::string& val, bool fallback) {
    const std::string low = toLowerAscii(trimWhitespace(val));
    if (low == "true" || low == "1") {
        return true;
    }
    if (low == "false" || low == "0") {
        return false;
    }
    return fallback;
}

uint32_t parseUintToken(const std::string& val, uint32_t fallback) {
    try {
        const long long parsed = std::stoll(trimWhitespace(val));
        return (parsed >= 0) ? static_cast<uint32_t>(parsed) : fallback;
    } catch (...) {
        return fallback;
    }
}

std::vector<std::string> parseExtensionArray(const std::string& val) {
    std::vector<std::string> out;
    const size_t lb = val.find('[');
    const size_t rb = val.rfind(']');
    if (lb == std::string::npos || rb == std::string::npos || rb <= lb) {
        return out;
    }
    std::istringstream iss(val.substr(lb + 1U, rb - lb - 1U));
    std::string item;
    while (std::getline(iss, item, ',')) {
        const std::string norm = normalizeExtension(item);
        if (!norm.empty()) {
            out.push_back(norm);
        }
    }
    return out;
}

GpuDecodeMode parseGpuMode(const std::string& val) {
    const std::string clean = toLowerAscii(stripQuotes(trimWhitespace(val)));
    if (clean == "available") {
        return GpuDecodeMode::Available;
    }
    if (clean == "disabled") {
        return GpuDecodeMode::Disabled;
    }
    return GpuDecodeMode::Recommended;
}

ThumbnailTimecodeMode parseThumbTimecodeMode(const std::string& val) {
    const std::string clean = toLowerAscii(stripQuotes(trimWhitespace(val)));
    if (clean == "hide_when_zero") {
        return ThumbnailTimecodeMode::HideWhenZero;
    }
    if (clean == "always_hide") {
        return ThumbnailTimecodeMode::AlwaysHide;
    }
    return ThumbnailTimecodeMode::AlwaysShow;
}

DurationCalculationMode parseDurationMode(const std::string& val) {
    const std::string clean = toLowerAscii(stripQuotes(trimWhitespace(val)));
    if (clean == "frame_timestamp") {
        return DurationCalculationMode::FrameTimestamp;
    }
    return DurationCalculationMode::Metadata;
}

void applyBoolOrIntConfig(const std::string& key, const std::string& val, OpenMediaConfig& cfg) {
    struct BoolField { const char* name; bool OpenMediaConfig::* ptr; };
    static constexpr std::array<BoolField, 6> kBoolFields = {{
        {"disable", &OpenMediaConfig::disable},
        {"overrideadobe", &OpenMediaConfig::overrideAdobe},
        {"disableautomaticoverride", &OpenMediaConfig::disableAutomaticOverride},
        {"reducewarnings", &OpenMediaConfig::reduceWarnings},
        {"disableseekingtable", &OpenMediaConfig::disableSeekingTable},
        {"disablethumbnailcache", &OpenMediaConfig::disableThumbnailCache},
    }};
    for (const auto& f : kBoolFields) {
        if (key == f.name) {
            cfg.*(f.ptr) = parseBoolToken(val, cfg.*(f.ptr));
            return;
        }
    }
    if (key == "maxframecachesizemb") cfg.maxFrameCacheSizeMB = parseUintToken(val, cfg.maxFrameCacheSizeMB);
    if (key == "maxpercentusageavailablememory") cfg.maxPercentUsageAvailableMemory = parseUintToken(val, cfg.maxPercentUsageAvailableMemory);
}

void applyConfigKeyValue(const std::string& rawKey, const std::string& val, OpenMediaConfig& cfg) {
    const std::string key = toLowerAscii(trimWhitespace(rawKey));
    if (key == "overrideextensions") {
        cfg.overrideExtensions = parseExtensionArray(val);
        return;
    }
    if (key == "gpudecodemode") {
        cfg.gpuDecodeMode = parseGpuMode(val);
        return;
    }
    if (key == "thumbnailtimecode") {
        cfg.thumbnailTimecode = parseThumbTimecodeMode(val);
        return;
    }
    if (key == "durationcalculation") {
        cfg.durationCalculation = parseDurationMode(val);
        return;
    }
    applyBoolOrIntConfig(key, val, cfg);
}

int32_t parseIntSafe(const std::string& s, int32_t fallback) {
    try {
        return std::stoi(s);
    } catch (...) {
        return fallback;
    }
}

openmedia::RationalTime parseFrameRateToken(const std::string& val) {
    const size_t dPos = val.find('d');
    if (dPos != std::string::npos) {
        const int32_t num = parseIntSafe(val.substr(0, dPos), 0);
        const int32_t den = parseIntSafe(val.substr(dPos + 1U), 1);
        return openmedia::RationalTime::sanitize(num, den);
    }
    const int32_t num = parseIntSafe(val, 0);
    return openmedia::RationalTime{num, 1};
}

std::string normalizePixelFormatToken(const std::string& raw) {
    const std::string low = toLowerAscii(raw);
    static constexpr std::array<const char*, 7> kAllowed = {
        "argb", "bgra", "bgra64", "yuv420p", "yuv420p10u", "uyvy422", "vuya"
    };
    for (const char* fmt : kAllowed) {
        if (low == fmt) {
            return low;
        }
    }
    return "";
}

void applyIntModifier(const std::string& key, const std::string& val, FilenameModifiers& out) {
    if (key == "w") out.width = std::max(0, parseIntSafe(val, out.width));
    if (key == "h") out.height = std::max(0, parseIntSafe(val, out.height));
    if (key == "vs") out.videoStreamIndex = parseIntSafe(val, out.videoStreamIndex);
    if (key == "as") out.audioStreamIndex = parseIntSafe(val, out.audioStreamIndex);
}

void applyColorAndFlagModifier(const std::string& key, const std::string& val, FilenameModifiers& out) {
    if (key == "cr" || key == "color_range") {
        const std::string lowVal = toLowerAscii(val);
        out.colorRange = (lowVal == "full") ? openmedia::ColorRange::Full
                       : (lowVal == "limited") ? openmedia::ColorRange::Limited : out.colorRange;
        return;
    }
    if (key == "accurate_duration") out.accurateDuration = true;
    if ((key == "rft" || key == "reset_first_timestamp") && (val.empty() || val == "1")) {
        out.resetFirstTimestamp = true;
    }
}

void applyModifierToken(const std::string& token, FilenameModifiers& out) {
    const size_t eq = token.find('=');
    const std::string key = toLowerAscii(eq == std::string::npos ? token : token.substr(0, eq));
    const std::string val = (eq == std::string::npos) ? "" : token.substr(eq + 1U);
    applyIntModifier(key, val, out);
    applyColorAndFlagModifier(key, val, out);
    if (key == "pf" || key == "pixel_format") {
        const std::string pf = normalizePixelFormatToken(val);
        if (!pf.empty()) out.pixelFormat = pf;
    }
    if (key == "fr" || key == "frame_rate") out.frameRate = parseFrameRateToken(val);
}

std::string stripVirtualSuffix(const std::string& path) {
    const std::string low = toLowerAscii(path);
    if (low.size() > 7U && low.substr(low.size() - 7U) == ".influx") {
        return path.substr(0, path.size() - 7U);
    }
    if (low.size() > 10U && low.substr(low.size() - 10U) == ".openmedia") {
        return path.substr(0, path.size() - 10U);
    }
    return path;
}

bool isPluginModernCodec(CodecId codec) noexcept {
    constexpr std::array<CodecId, 6> kModernCodecs{
        CodecId::AV1, CodecId::VP9, CodecId::VP8,
        CodecId::Opus, CodecId::FLAC, CodecId::Vorbis
    };
    return std::find(kModernCodecs.begin(), kModernCodecs.end(), codec) != kModernCodecs.end();
}

const StreamInfo* findFirstStream(const std::vector<StreamInfo>& streams, MediaType type) noexcept {
    for (const auto& stream : streams) {
        if (stream.type == type) {
            return &stream;
        }
    }
    return nullptr;
}

void populateVideoStreamInfo(const StreamInfo& video, ImFileInfoRec8Ext& infoRec) noexcept {
    infoRec.hasVideo = kPrTrueVal;
    infoRec.vidInfo.imageWidth = video.width;
    infoRec.vidInfo.imageHeight = video.height;
    infoRec.vidInfo.pixelAspectNum = 1U;
    infoRec.vidInfo.pixelAspectDen = 1U;
    infoRec.vidDuration = (video.durationPts > 0) ? static_cast<csSDK_uint32>(video.durationPts) : 0U;

    const int64_t rawNum = video.frameRate.num;
    const int64_t rawDen = video.frameRate.den;
    const int64_t safeDen = (rawDen <= 0) ? 1 : rawDen;
    infoRec.vidScale = (rawNum > 0) ? static_cast<csSDK_int32>(rawNum) : 30;
    infoRec.vidSampleSize = (rawNum > 0) ? static_cast<csSDK_int32>(safeDen) : 1;
}

void populateAudioStreamInfo(const StreamInfo& audio, ImFileInfoRec8Ext& infoRec) noexcept {
    infoRec.hasAudio = kPrTrueVal;
    infoRec.audSampleRate = (audio.sampleRate > 0) ? static_cast<double>(audio.sampleRate) : 48000.0;
    infoRec.audNumChannels = audio.channels;
    infoRec.audDuration = (audio.durationPts > 0) ? static_cast<csSDK_uint64>(audio.durationPts) : 0ULL;
}

} // namespace

OpenMediaConfig OpenMediaConfig::parseConfigToml(const std::string& tomlContent) {
    OpenMediaConfig cfg;
    std::istringstream stream(tomlContent);
    std::string line;
    while (std::getline(stream, line)) {
        const std::string clean = trimWhitespace(stripTomlComment(line));
        const size_t eq = clean.find('=');
        if (clean.empty() || clean.front() == '[' || eq == std::string::npos) {
            continue;
        }
        applyConfigKeyValue(clean.substr(0, eq), clean.substr(eq + 1U), cfg);
    }
    return cfg;
}

std::string OpenMediaConfig::resolveDefaultConfigPath() {
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    const std::string base = (appData != nullptr) ? appData : ".";
    return base + "\\OpenMedia\\Config.toml";
#else
    const char* home = std::getenv("HOME");
    const std::string base = (home != nullptr) ? home : ".";
    return base + "/.config/OpenMedia/Config.toml";
#endif
}

OpenMediaConfig OpenMediaConfig::loadDefaultConfig() {
    std::ifstream ifs(resolveDefaultConfigPath());
    if (!ifs.is_open()) {
        return OpenMediaConfig{};
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return parseConfigToml(oss.str());
}

bool OpenMediaConfig::isExtensionOverridden(const std::string& ext) const {
    const std::string norm = normalizeExtension(ext);
    return std::find(overrideExtensions.begin(), overrideExtensions.end(), norm) != overrideExtensions.end();
}

FilenameModifiers FilenameModifiers::parseFilenameModifiers(const std::string& path) {
    FilenameModifiers mods;
    const std::string basePath = stripVirtualSuffix(path);
    const size_t slashPos = basePath.find_last_of("/\\");
    const size_t dotPos = basePath.find_last_of('.');
    const size_t searchFrom = (slashPos == std::string::npos) ? 0U : (slashPos + 1U);
    const size_t bangPos = basePath.find('!', searchFrom);
    if (bangPos == std::string::npos || (slashPos != std::string::npos && bangPos < slashPos)) {
        mods.cleanPath = basePath;
        return mods;
    }

    const size_t modEnd = (dotPos != std::string::npos && dotPos > bangPos) ? dotPos : basePath.size();
    const std::string extPart = (dotPos != std::string::npos && dotPos > bangPos) ? basePath.substr(dotPos) : "";
    mods.cleanPath = basePath.substr(0, bangPos) + extPart;

    std::istringstream iss(basePath.substr(bangPos + 1U, modEnd - bangPos - 1U));
    std::string token;
    while (std::getline(iss, token, '!')) {
        if (!token.empty()) {
            applyModifierToken(token, mods);
        }
    }
    return mods;
}

void FilenameModifiers::resolveAspectRatio(int32_t srcWidth,
                                           int32_t srcHeight,
                                           int32_t& ioWidth,
                                           int32_t& ioHeight) noexcept {
    if (srcWidth <= 0 || srcHeight <= 0) {
        return;
    }
    if (ioWidth > 0 && ioHeight <= 0) {
        ioHeight = static_cast<int32_t>((static_cast<int64_t>(ioWidth) * srcHeight) / srcWidth);
        return;
    }
    if (ioHeight > 0 && ioWidth <= 0) {
        ioWidth = static_cast<int32_t>((static_cast<int64_t>(ioHeight) * srcWidth) / srcHeight);
    }
}

bool ErrorDeduplicator::shouldLogWarning(const std::string& fileInstanceKey,
                                         const std::string& warningKey,
                                         bool reduceWarnings) {
    if (!reduceWarnings) {
        return true;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    return seenWarnings_.insert(fileInstanceKey + "::" + warningKey).second;
}

void ErrorDeduplicator::clearFile(const std::string& fileInstanceKey) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string prefix = fileInstanceKey + "::";
    for (auto it = seenWarnings_.begin(); it != seenWarnings_.end();) {
        it = (it->compare(0, prefix.size(), prefix) == 0) ? seenWarnings_.erase(it) : std::next(it);
    }
}

void ErrorDeduplicator::clearAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    seenWarnings_.clear();
}

std::string FileProbe::utf16ToUtf8(const std::u16string& utf16) {
    std::string out;
    out.reserve(utf16.size());
    for (size_t i = 0; i < utf16.size(); ++i) {
        appendCodePointUtf8(decodeNextCodePoint(utf16, i), out);
    }
    return out;
}

bool FileProbe::shouldDelegateToNative(const std::vector<StreamInfo>& streams, const std::string& ext) {
    const std::string cleanExt = normalizeExtension(ext);
    if ((cleanExt != "mp4" && cleanExt != "mov") || streams.empty()) {
        return false;
    }
    return std::none_of(streams.begin(), streams.end(), [](const StreamInfo& stream) {
        return isPluginModernCodec(stream.codec);
    });
}

csSDK_int32 FileProbe::openAndProbe(ImOpenFileRec8Ext& openRec, ICoreDemuxer& demuxer) {
    const std::string utf8Path = utf16ToUtf8(openRec.fileinfo.filepath);
    if (utf8Path.empty()) {
        return kImBadFile;
    }
    const FilenameModifiers mods = FilenameModifiers::parseFilenameModifiers(utf8Path);
    if (demuxer.openFile(mods.cleanPath) != CoreStatus::Ok) {
        return kImBadFile;
    }
    const std::string ext = extractExtension(mods.cleanPath);
    if (shouldDelegateToNative(demuxer.getStreams(), ext)) {
        demuxer.close();
        return kImBadFile;
    }
    return kImNoErr;
}

csSDK_int32 FileProbe::populateFileInfo(const std::vector<StreamInfo>& streams, ImFileInfoRec8Ext& infoRec) {
    if (streams.empty()) {
        return kImBadFile;
    }
    infoRec.hasVideo = kPrFalseVal;
    infoRec.hasAudio = kPrFalseVal;
    infoRec.vidInfo.supportsGetSourceVideo = kPrTrueVal;
    infoRec.vidInfo.supportsAsyncIO = kPrTrueVal;
    infoRec.accessModes |= kRandomAccessImportFlag;
    infoRec.highMemUsage = kPrTrueVal;

    if (const StreamInfo* video = findFirstStream(streams, MediaType::Video)) {
        populateVideoStreamInfo(*video, infoRec);
    }
    if (const StreamInfo* audio = findFirstStream(streams, MediaType::Audio)) {
        populateAudioStreamInfo(*audio, infoRec);
    }
    return kImNoErr;
}

} // namespace openmedia::importer
