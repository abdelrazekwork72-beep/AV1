/// @file file_probe.cpp
/// @brief Implementation of FileProbe for imOpenFile8 and imGetInfo8.

#include "importer/file_probe.hpp"

#include <algorithm>
#include <array>
#include <cctype>

namespace openmedia::importer {
namespace {

/// @brief Appends a Unicode code point encoded as UTF-8 to the output string.
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

/// @brief Checks whether a UTF-16 code unit is a high surrogate (0xD800..0xDBFF).
bool isHighSurrogate(char16_t unit) noexcept {
    return unit >= 0xD800U && unit <= 0xDBFFU;
}

/// @brief Checks whether a UTF-16 code unit is a low surrogate (0xDC00..0xDFFF).
bool isLowSurrogate(char16_t unit) noexcept {
    return unit >= 0xDC00U && unit <= 0xDFFFU;
}

/// @brief Decodes the next Unicode code point from UTF-16, advancing index past any surrogate pair.
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

/// @brief Normalizes a file extension by stripping a leading dot and lowercasing ASCII characters.
std::string normalizeExtension(const std::string& ext) {
    std::string clean = (!ext.empty() && ext.front() == '.') ? ext.substr(1U) : ext;
    std::transform(clean.begin(), clean.end(), clean.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return clean;
}

/// @brief Extracts the lowercase file extension from a UTF-8 path.
std::string extractExtension(const std::string& path) {
    const size_t slashPos = path.find_last_of("/\\");
    const size_t dotPos = path.find_last_of('.');
    if (dotPos == std::string::npos || (slashPos != std::string::npos && dotPos < slashPos)) {
        return "";
    }
    return normalizeExtension(path.substr(dotPos + 1U));
}

/// @brief Returns true if the codec requires OpenMedia Importer rather than native Adobe fallback.
bool isPluginModernCodec(CodecId codec) noexcept {
    constexpr std::array<CodecId, 6> kModernCodecs{
        CodecId::AV1, CodecId::VP9, CodecId::VP8,
        CodecId::Opus, CodecId::FLAC, CodecId::Vorbis
    };
    return std::find(kModernCodecs.begin(), kModernCodecs.end(), codec) != kModernCodecs.end();
}

/// @brief Finds the first stream matching the requested MediaType, or nullptr if none exists.
const StreamInfo* findFirstStream(const std::vector<StreamInfo>& streams, MediaType type) noexcept {
    for (const auto& stream : streams) {
        if (stream.type == type) {
            return &stream;
        }
    }
    return nullptr;
}

/// @brief Populates video stream fields and sanitized rational timebase on ImFileInfoRec8Ext.
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

/// @brief Populates audio stream fields on ImFileInfoRec8Ext.
void populateAudioStreamInfo(const StreamInfo& audio, ImFileInfoRec8Ext& infoRec) noexcept {
    infoRec.hasAudio = kPrTrueVal;
    infoRec.audSampleRate = (audio.sampleRate > 0) ? static_cast<double>(audio.sampleRate) : 48000.0;
    infoRec.audNumChannels = audio.channels;
    infoRec.audDuration = (audio.durationPts > 0) ? static_cast<csSDK_uint64>(audio.durationPts) : 0ULL;
}

} // namespace

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
    if (utf8Path.empty() || demuxer.openFile(utf8Path) != CoreStatus::Ok) {
        return kImBadFile;
    }
    const std::string ext = extractExtension(utf8Path);
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
