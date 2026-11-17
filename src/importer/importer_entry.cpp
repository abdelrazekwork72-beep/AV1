/// @file importer_entry.cpp
/// @brief Implementation of xOpenMediaImportEntry, xImportEntry, 39-format dispatch,
///        real Adobe Premiere Pro / Media Encoder x64 binary ABI (v3.0.2) + CTest bridge,
///        real audio/video decoding, frame-accurate seeking, and OpenMediaDesktop popup integration.

#include "importer/importer_entry.hpp"
#include "importer/async_importer.hpp"
#include "importer/audio_extractor.hpp"
#include "importer/file_probe.hpp"
#include "importer/frame_scaler.hpp"
#include "importer/keyframe_ring_buffer.hpp"
#include "importer/memory_governor.hpp"
#include "importer/multi_channel_router.hpp"
#include "importer/peak_waveform_generator.hpp"
#include "importer/pixel_format_8u.hpp"
#include "importer/pixel_format_hdr.hpp"
#include "importer/source_video.hpp"
#include "importer/stream_tracker.hpp"
#include "openmedia/audio_decoder.hpp"
#include "openmedia/demuxer.hpp"
#include "openmedia/video_decoder.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

namespace {

void preloadCompanionDllsFromModuleDir(HMODULE hModule) noexcept {
    wchar_t modulePath[MAX_PATH]{};
    if (GetModuleFileNameW(hModule, modulePath, MAX_PATH) == 0) {
        return;
    }
    std::wstring dir(modulePath);
    const size_t slash = dir.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return;
    }
    dir.resize(slash);
    SetDllDirectoryW(dir.c_str());
    AddDllDirectory(dir.c_str());

    static constexpr const wchar_t* kCompanionDlls[] = {
        L"avutil-60.dll",
        L"swresample-6.dll",
        L"swscale-9.dll",
        L"avcodec-62.dll",
        L"avformat-62.dll",
        L"OpenMediaCore.dll"
    };
    for (const wchar_t* dllName : kCompanionDlls) {
        const std::wstring fullPath = dir + L"\\" + dllName;
        if (GetFileAttributesW(fullPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            LoadLibraryExW(fullPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        }
    }
}

} // namespace

#endif

namespace openmedia::importer {
namespace {

constexpr uint32_t makeFourCC(char a, char b, char c, char d) noexcept {
    return (static_cast<uint32_t>(static_cast<uint8_t>(a)) << 24U) |
           (static_cast<uint32_t>(static_cast<uint8_t>(b)) << 16U) |
           (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 8U) |
           static_cast<uint32_t>(static_cast<uint8_t>(d));
}

struct SupportedFormatDesc {
    csSDK_uint32 distinctFourCC;
    const char* formatName;
    const char* formatShortName;
    const char* extension;
};

constexpr std::array<SupportedFormatDesc, 39> kSupportedFormats = {{
    {makeFourCC('M','K','V',' '), "Matroska Video (OpenMedia)", "Matroska", "mkv"},
    {makeFourCC('W','E','B','M'), "WebM Video (OpenMedia)", "WebM", "webm"},
    {makeFourCC('O','G','G',' '), "Ogg Media (OpenMedia)", "Ogg", "ogg"},
    {makeFourCC('F','L','V',' '), "Flash Video (OpenMedia)", "FLV", "flv"},
    {makeFourCC('A','V','I',' '), "AVI Video (OpenMedia)", "AVI", "avi"},
    {makeFourCC('M','P','4',' '), "MPEG-4 Video (OpenMedia)", "MP4", "mp4"},
    {makeFourCC('M','O','V',' '), "QuickTime Movie (OpenMedia)", "QuickTime", "mov"},
    {makeFourCC('O','G','V',' '), "Ogg Video (OpenMedia)", "OGV", "ogv"},
    {makeFourCC('M','4','V',' '), "M4V Video (OpenMedia)", "M4V", "m4v"},
    {makeFourCC('M','P','G',' '), "MPEG Program Stream (OpenMedia)", "MPG", "mpg"},
    {makeFourCC('M','P','E','G'), "MPEG Video (OpenMedia)", "MPEG", "mpeg"},
    {makeFourCC('T','S',' ',' '), "MPEG Transport Stream (OpenMedia)", "TS", "ts"},
    {makeFourCC('M','2','T','S'), "Blu-ray Transport Stream (OpenMedia)", "M2TS", "m2ts"},
    {makeFourCC('M','T','S',' '), "AVCHD Transport Stream (OpenMedia)", "MTS", "mts"},
    {makeFourCC('V','O','B',' '), "DVD Video Object (OpenMedia)", "VOB", "vob"},
    {makeFourCC('M','X','F',' '), "Material Exchange Format (OpenMedia)", "MXF", "mxf"},
    {makeFourCC('W','M','V',' '), "Windows Media Video (OpenMedia)", "WMV", "wmv"},
    {makeFourCC('W','M','A',' '), "Windows Media Audio (OpenMedia)", "WMA", "wma"},
    {makeFourCC('A','S','F',' '), "Advanced Systems Format (OpenMedia)", "ASF", "asf"},
    {makeFourCC('B','I','K',' '), "Bink Video (OpenMedia)", "BIK", "bik"},
    {makeFourCC('3','G','P',' '), "3GPP Multimedia (OpenMedia)", "3GP", "3gp"},
    {makeFourCC('3','6','0',' '), "GoPro MAX 360 (OpenMedia)", "360", "360"},
    {makeFourCC('Y','4','M',' '), "YUV4MPEG2 Raw (OpenMedia)", "Y4M", "y4m"},
    {makeFourCC('M','J','P','G'), "Motion JPEG (OpenMedia)", "MJPEG", "mjpeg"},
    {makeFourCC('A','P','N','G'), "Animated PNG (OpenMedia)", "APNG", "apng"},
    {makeFourCC('W','E','B','P'), "WebP Image (OpenMedia)", "WebP", "webp"},
    {makeFourCC('Q','O','I',' '), "Quite OK Image (OpenMedia)", "QOI", "qoi"},
    {makeFourCC('G','I','F',' '), "GIF Animation (OpenMedia)", "GIF", "gif"},
    {makeFourCC('F','L','A','C'), "Free Lossless Audio Codec (OpenMedia)", "FLAC", "flac"},
    {makeFourCC('O','P','U','S'), "Opus Audio (OpenMedia)", "Opus", "opus"},
    {makeFourCC('M','K','A',' '), "Matroska Audio (OpenMedia)", "MKA", "mka"},
    {makeFourCC('W','A','V',' '), "Waveform Audio (OpenMedia)", "WAV", "wav"},
    {makeFourCC('A','A','C',' '), "Advanced Audio Coding (OpenMedia)", "AAC", "aac"},
    {makeFourCC('M','P','3',' '), "MPEG Layer III Audio (OpenMedia)", "MP3", "mp3"},
    {makeFourCC('A','C','3',' '), "Dolby AC-3 Audio (OpenMedia)", "AC3", "ac3"},
    {makeFourCC('D','T','S',' '), "DTS-HD Audio (OpenMedia)", "DTS", "dts"},
    {makeFourCC('A','I','F','F'), "Audio Interchange File (OpenMedia)", "AIFF", "aiff"},
    {makeFourCC('I','N','F','L'), "OpenMedia Forced Container (Influx)", "INFLUX", "influx"},
    {kOpenMediaImporterFourCC,    "OpenMedia Forced Container", "OPENMEDIA", "openmedia"},
}};

constexpr size_t kMaxOpenSessions = 32U;
constexpr uint64_t kSessionBoxMagic = 0x05D5C6E9ULL;

bool isRealAdobeHostCall(const imStdParms* stdParms) noexcept {
    if (stdParms == nullptr) {
        return false;
    }
    const auto* raw = reinterpret_cast<const PrRawStdParms*>(stdParms);
    return (raw->imInterfaceVer >= 6 && raw->piSuites != nullptr);
}

PrHostMemFuncs* resolveHostMemFuncs(const imStdParms* stdParms) noexcept {
    if (!isRealAdobeHostCall(stdParms)) {
        return nullptr;
    }
    const auto* raw = reinterpret_cast<const PrRawStdParms*>(stdParms);
    return (raw->piSuites != nullptr) ? raw->piSuites->memFuncs : nullptr;
}

SPBasicSuite* resolveHostSPBasicSuite(const imStdParms* stdParms) noexcept {
    if (!isRealAdobeHostCall(stdParms)) {
        return nullptr;
    }
    const auto* raw = reinterpret_cast<const PrRawStdParms*>(stdParms);
    if (raw->piSuites != nullptr && raw->piSuites->plugSuites != nullptr &&
        raw->piSuites->plugSuites->getSPBasicSuite != nullptr) {
        return raw->piSuites->plugSuites->getSPBasicSuite();
    }
    return nullptr;
}

struct ImporterFileSession;

struct SessionBox {
    uint64_t magic = kSessionBoxMagic;
    ImporterFileSession* sessionPtr = nullptr;
    SessionBox* selfPtr = nullptr;
    char** hostHandle = nullptr;
    char* hostPrefsPtr = nullptr;
};

struct ImporterFileSession {
    std::unique_ptr<CoreDemuxer> demuxer = std::make_unique<CoreDemuxer>();
    std::unique_ptr<CoreVideoDecoder> videoDecoder = std::make_unique<CoreVideoDecoder>();
    std::unique_ptr<CoreAudioDecoder> audioDecoder = std::make_unique<CoreAudioDecoder>();
    FilenameModifiers mods{};
    std::vector<StreamInfo> effectiveStreams;
    KeyframeRingBuffer keyframeBuffer{32U};
    RawVideoFrame lastDecodedFrame{};
    bool hasLastDecodedFrame = false;
    bool videoInitialized = false;
    bool audioInitialized = false;
    int32_t videoStreamIndex = -1;
    int32_t audioStreamIndex = -1;
    uint32_t audioChannels = 2U;
    int32_t audioSampleRate = 48000;
    int32_t vidScale = 30;
    int32_t vidSampleSize = 1;
    int64_t lastVideoFrameIdx = -1;
    int64_t lastAudioSamplePos = 0;
    SessionBox box{};
    std::chrono::steady_clock::time_point lastAccess = std::chrono::steady_clock::now();
};

std::mutex& sessionRegistryMutex() {
    static std::mutex mtx;
    return mtx;
}

std::unordered_map<void*, std::shared_ptr<ImporterFileSession>>& sessionRegistry() {
    static std::unordered_map<void*, std::shared_ptr<ImporterFileSession>> map;
    return map;
}

std::weak_ptr<ImporterFileSession>& lastActiveSessionRef() {
    static std::weak_ptr<ImporterFileSession> last;
    return last;
}

void* allocateHostSessionHandle(const imStdParms* stdParms, ImporterFileSession& session) noexcept {
    session.box.magic = kSessionBoxMagic;
    session.box.sessionPtr = &session;
    session.box.selfPtr = &session.box;
    if (PrHostMemFuncs* mem = resolveHostMemFuncs(stdParms)) {
        if (mem->newHandle != nullptr && session.box.hostHandle == nullptr) {
            char** h = mem->newHandle(16U);
            if (h != nullptr) {
                if (mem->lockHandle != nullptr) {
                    mem->lockHandle(h);
                }
                if (*h != nullptr) {
                    std::memcpy(*h, &session.box.magic, sizeof(uint64_t));
                    ImporterFileSession* ptr = &session;
                    std::memcpy(*h + 8, &ptr, sizeof(ImporterFileSession*));
                }
                session.box.hostHandle = h;
                return static_cast<void*>(h);
            }
        }
    }
    return static_cast<void*>(&session.box.selfPtr);
}

void registerSessionKeysLocked(const std::shared_ptr<ImporterFileSession>& session, void* primaryKey) {
    if (!session) {
        return;
    }
    auto& reg = sessionRegistry();
    reg[session.get()] = session;
    reg[&session->box] = session;
    reg[&session->box.selfPtr] = session;
    if (primaryKey != nullptr) {
        reg[primaryKey] = session;
    }
    if (session->box.hostHandle != nullptr) {
        reg[session->box.hostHandle] = session;
        if (*session->box.hostHandle != nullptr) {
            reg[*session->box.hostHandle] = session;
        }
    }
    if (session->box.hostPrefsPtr != nullptr) {
        reg[session->box.hostPrefsPtr] = session;
    }
    lastActiveSessionRef() = session;
}

std::shared_ptr<ImporterFileSession> findSession(void* key, bool allowFallback = false) {
    std::lock_guard<std::mutex> lock(sessionRegistryMutex());
    if (key != nullptr) {
        const auto it = sessionRegistry().find(key);
        if (it != sessionRegistry().end() && it->second) {
            it->second->lastAccess = std::chrono::steady_clock::now();
            return it->second;
        }
    }
    if (allowFallback) {
        if (auto fallback = lastActiveSessionRef().lock()) {
            fallback->lastAccess = std::chrono::steady_clock::now();
            return fallback;
        }
    }
    return nullptr;
}

void evictOldestSessionsLocked() {
    auto& reg = sessionRegistry();
    while (reg.size() >= kMaxOpenSessions * 4U) {
        auto oldest = reg.begin();
        for (auto it = reg.begin(); it != reg.end(); ++it) {
            if (it->second && oldest->second && it->second->lastAccess < oldest->second->lastAccess) {
                oldest = it;
            }
        }
        auto target = oldest->second;
        if (target && target->demuxer) {
            target->demuxer->quietFile();
        }
        for (auto it = reg.begin(); it != reg.end();) {
            if (it->second == target) {
                it = reg.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void initSessionDecoders(ImporterFileSession& session) {
    int32_t vidSeen = 0;
    int32_t audSeen = 0;
    for (const auto& st : session.effectiveStreams) {
        if (st.type == MediaType::Video) {
            const bool matchVid = (session.mods.videoStreamIndex < 0) ||
                                  (vidSeen == session.mods.videoStreamIndex);
            if (!session.videoInitialized && matchVid) {
                session.videoStreamIndex = st.index;
                session.vidScale = (st.frameRate.num > 0) ? static_cast<int32_t>(st.frameRate.num) : 30;
                session.vidSampleSize = (st.frameRate.den > 0) ? static_cast<int32_t>(st.frameRate.den) : 1;
                session.videoInitialized = (session.videoDecoder->initialize(st) == CoreStatus::Ok);
            }
            ++vidSeen;
        } else if (st.type == MediaType::Audio) {
            const bool matchAud = (session.mods.audioStreamIndex < 0) ||
                                  (audSeen == session.mods.audioStreamIndex);
            if (!session.audioInitialized && matchAud) {
                session.audioStreamIndex = st.index;
                session.audioChannels = (st.channels > 0) ? static_cast<uint32_t>(st.channels) : 2U;
                session.audioSampleRate = (st.sampleRate > 0) ? st.sampleRate : 48000;
                session.audioInitialized = (session.audioDecoder->initialize(st, session.audioSampleRate) == CoreStatus::Ok);
                if (!session.audioInitialized && !session.videoInitialized) {
                    session.demuxer->close();
                }
            }
            ++audSeen;
        }
    }
}

bool decodeNextVideoFrameFromSession(ImporterFileSession& session, RawVideoFrame& outFrame) {
    if (!session.videoInitialized || session.videoStreamIndex < 0) {
        return false;
    }
    constexpr int32_t kMaxPacketsToRead = 256;
    for (int32_t i = 0; i < kMaxPacketsToRead; ++i) {
        if (session.videoDecoder->decodeNextFrame(outFrame) == CoreStatus::Ok) {
            session.lastDecodedFrame = outFrame;
            session.hasLastDecodedFrame = true;
            return true;
        }
        EncodedPacket pkt{};
        const CoreStatus pktStatus = session.demuxer->readNextPacket(pkt);
        if (pktStatus == CoreStatus::EndOfFile) {
            session.videoDecoder->flush();
            if (session.videoDecoder->decodeNextFrame(outFrame) == CoreStatus::Ok) {
                session.lastDecodedFrame = outFrame;
                session.hasLastDecodedFrame = true;
                return true;
            }
            break;
        }
        if (pktStatus != CoreStatus::Ok) {
            break;
        }
        if (pkt.streamIndex == session.videoStreamIndex) {
            if (pkt.isKeyFrame) {
                session.keyframeBuffer.addKeyframe(KeyframeEntry{pkt.pts, session.lastVideoFrameIdx, 0, false});
            }
            session.videoDecoder->sendPacket(pkt);
        }
    }
    if (session.hasLastDecodedFrame) {
        outFrame = session.lastDecodedFrame;
        return true;
    }
    return false;
}

bool decodeNextAudioSamples(ImporterFileSession& session,
                            size_t targetSamples,
                            uint32_t channels,
                            std::vector<int16_t>& outInterleaved) {
    if (!session.audioInitialized || session.audioStreamIndex < 0 || channels == 0) {
        return false;
    }
    RawAudioBatch batch{};
    constexpr int32_t kMaxPacketsPerCall = 128;
    for (int32_t p = 0; p < kMaxPacketsPerCall; ++p) {
        if (session.audioDecoder->decodeAudioBatch(static_cast<int32_t>(targetSamples), batch) == CoreStatus::Ok &&
            batch.numSamples > 0 && !batch.channelData.empty()) {
            const size_t avail = static_cast<size_t>(batch.numSamples);
            outInterleaved.resize(avail * channels);
            for (size_t i = 0; i < avail; ++i) {
                for (uint32_t ch = 0; ch < channels; ++ch) {
                    const size_t srcCh = (std::min<size_t>)(ch, batch.channelData.size() - 1U);
                    const float sample = (i < batch.channelData[srcCh].size())
                                             ? std::clamp(batch.channelData[srcCh][i], -1.0f, 1.0f)
                                             : 0.0f;
                    outInterleaved[i * channels + ch] = static_cast<int16_t>(sample * 32767.0f);
                }
            }
            return true;
        }
        EncodedPacket pkt{};
        const CoreStatus pktStatus = session.demuxer->readNextPacket(pkt);
        if (pktStatus != CoreStatus::Ok) {
            break;
        }
        if (pkt.streamIndex == session.audioStreamIndex) {
            session.audioDecoder->sendPacket(pkt);
        }
    }
    return false;
}

csSDK_int32 handleImGetSupports7(imStdParms* /*stdParms*/, void* /*param1*/, void* /*param2*/) {
    return kMalSupports7;
}

csSDK_int32 handleImGetSupports8(imStdParms* /*stdParms*/, void* /*param1*/, void* /*param2*/) {
    return kMalSupports8;
}

csSDK_int32 handleImShutdown(imStdParms* /*stdParms*/, void* /*param1*/, void* /*param2*/) {
    std::lock_guard<std::mutex> lock(sessionRegistryMutex());
    sessionRegistry().clear();
    return kImNoErr;
}

csSDK_int32 handleImOpenFile8(imStdParms* stdParms, void* param1, void* param2) {
    if (isRealAdobeHostCall(stdParms)) {
        // In real Premiere Pro x64 ABI (verified at 0x18000d68d & 0x18000fb00 in InfluxAdobeBase.dll):
        // param1 (rdx) = imFileRef* (Windows HANDLE*), param2 (r8) = imFileOpenRec8*
        auto* rawOpen = static_cast<PrRawOpenFileRefRec8*>(param2 != nullptr ? param2 : param1);
        if (rawOpen == nullptr) {
            return kImMemErr;
        }
        const char16_t* wpath = rawOpen->filepathPtr;
        const std::u16string u16Path(wpath != nullptr ? wpath : u"");
        auto session = std::make_shared<ImporterFileSession>();
        const std::string utf8Path = FileProbe::utf16ToUtf8(u16Path);
        session->mods = FilenameModifiers::parseFilenameModifiers(utf8Path);
        if (session->mods.resetFirstTimestamp) {
            session->demuxer->setResetFirstTimestamp(true);
        }
        ImOpenFileRec8Ext bridgeOpen{};
        bridgeOpen.fileinfo.filepath = u16Path;
        const csSDK_int32 status = FileProbe::openAndProbe(bridgeOpen, *session->demuxer);
        if (status != kImNoErr) {
            session->demuxer->close();
            return status;
        }
#ifdef _WIN32
        if (!u16Path.empty()) {
            const DWORD accessMode = (rawOpen->inReadWrite == 2) ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_READ;
            HANDLE hFile = CreateFileW(reinterpret_cast<LPCWSTR>(u16Path.c_str()),
                                       accessMode,
                                       FILE_SHARE_READ | FILE_SHARE_WRITE,
                                       nullptr,
                                       OPEN_EXISTING,
                                       FILE_ATTRIBUTE_NORMAL,
                                       nullptr);
            if (hFile != INVALID_HANDLE_VALUE) {
                rawOpen->outFileRef = hFile;
                if (param1 != nullptr && param1 != param2) {
                    *static_cast<HANDLE*>(param1) = hFile;
                }
            }
        }
#endif
        rawOpen->sdkMagic = 0x53444B5FU; // '_KDS'
        session->effectiveStreams = session->demuxer->getStreams();
        FileProbe::applyModifiersToStreams(session->mods, session->effectiveStreams);
        initSessionDecoders(*session);

        void* handleKey = (rawOpen->privatedata != nullptr)
                              ? rawOpen->privatedata
                              : allocateHostSessionHandle(stdParms, *session);
        rawOpen->privatedata = handleKey;
        std::lock_guard<std::mutex> lock(sessionRegistryMutex());
        evictOldestSessionsLocked();
        registerSessionKeysLocked(session, handleKey);
        return kImNoErr;
    }

    if (param1 == nullptr) {
        return kImMemErr;
    }
    auto* openRec = static_cast<ImOpenFileRec8Ext*>(param1);
    auto session = std::make_shared<ImporterFileSession>();
    const std::string utf8Path = FileProbe::utf16ToUtf8(openRec->fileinfo.filepath);
    session->mods = FilenameModifiers::parseFilenameModifiers(utf8Path);
    if (session->mods.resetFirstTimestamp) {
        session->demuxer->setResetFirstTimestamp(true);
    }
    const csSDK_int32 status = FileProbe::openAndProbe(*openRec, *session->demuxer);
    if (status != kImNoErr) {
        session->demuxer->close();
        return status;
    }
    session->effectiveStreams = session->demuxer->getStreams();
    FileProbe::applyModifiersToStreams(session->mods, session->effectiveStreams);
    initSessionDecoders(*session);
    MemoryGovernor::applyMemoryFlags(MemoryGovernor::queryAvailablePhysicalRamBytes(), openRec, nullptr);
    void* handleKey = session.get();
    openRec->privatedata = handleKey;
    std::lock_guard<std::mutex> lock(sessionRegistryMutex());
    evictOldestSessionsLocked();
    registerSessionKeysLocked(session, handleKey);
    return kImNoErr;
}

void writeRawFileInfo8(const imStdParms* stdParms,
                       uint8_t* rawInfo,
                       const ImFileInfoRec8Ext& infoRec,
                       ImporterFileSession& session,
                       void* sessionKey) noexcept {
    rawInfo[0x00] = infoRec.hasVideo ? 1U : 0U;
    rawInfo[0x01] = infoRec.hasAudio ? 1U : 0U;
    if (infoRec.hasVideo) {
        std::memcpy(rawInfo + 0x02, &infoRec.vidInfo.imageWidth, sizeof(csSDK_int32));
        std::memcpy(rawInfo + 0x06, &infoRec.vidInfo.imageHeight, sizeof(csSDK_int32));
        const csSDK_int32 depth = -1; // 0xFFFFFFFF (verified at 0x18002e12f in InfluxAdobeBase.dll)
        std::memcpy(rawInfo + 0x0E, &depth, sizeof(csSDK_int32));
        rawInfo[0x12] = 0; // prFieldsNone
        rawInfo[0x16] = 5; // alphaNone = 5 (verified at 0x18002e14c in InfluxAdobeBase.dll)
        std::memcpy(rawInfo + 0x22, &infoRec.vidInfo.pixelAspectNum, sizeof(csSDK_uint32));
        std::memcpy(rawInfo + 0x26, &infoRec.vidInfo.pixelAspectDen, sizeof(csSDK_uint32));
        const csSDK_int32 one = 1;
        std::memcpy(rawInfo + 0x36, &one, sizeof(csSDK_int32)); // supportsGetSourceVideo = 1
        std::memcpy(rawInfo + 0x62, &one, sizeof(csSDK_int32));
        std::memcpy(rawInfo + 0xA2, &infoRec.vidScale, sizeof(csSDK_int32));
        std::memcpy(rawInfo + 0xA6, &infoRec.vidSampleSize, sizeof(csSDK_int32));
        csSDK_int64 dur64 = (infoRec.vidDuration > 0)
                                ? static_cast<csSDK_int64>(infoRec.vidDuration)
                                : 1LL;
        for (const auto& st : session.effectiveStreams) {
            if (st.type == openmedia::MediaType::Video && st.durationPts > 0 &&
                st.timeBase.num > 0 && st.timeBase.den > 0 &&
                st.frameRate.num > 0 && st.frameRate.den > 0) {
                const double sec = static_cast<double>(st.durationPts) *
                                   static_cast<double>(st.timeBase.num) /
                                   static_cast<double>(st.timeBase.den);
                const double frames = sec * static_cast<double>(st.frameRate.num) /
                                      static_cast<double>(st.frameRate.den);
                if (frames >= 1.0) {
                    dur64 = static_cast<csSDK_int64>(frames + 0.5);
                }
                break;
            }
        }
        std::memcpy(rawInfo + 0x14E5, &dur64, sizeof(csSDK_int64));

        // Populate PrSDKString codec/subtype name at +0x52 via "MediaCore StringSuite" v1
        if (SPBasicSuite* basic = resolveHostSPBasicSuite(stdParms)) {
            if (basic->AcquireSuite != nullptr) {
                const PrHostStringSuiteV1* strSuite = nullptr;
                if (basic->AcquireSuite("MediaCore StringSuite", 1,
                                        reinterpret_cast<const void**>(&strSuite)) == 0 &&
                    strSuite != nullptr && strSuite->AllocateFromUTF8 != nullptr) {
                    strSuite->AllocateFromUTF8(
                        reinterpret_cast<const std::uint8_t*>("OpenMedia"),
                        rawInfo + 0x52);
                }
            }
        }
    }

    const csSDK_int32 zero32 = 0;
    std::memcpy(rawInfo + 0x32, &zero32, sizeof(csSDK_int32));
    rawInfo[0xD6] = 0;
    rawInfo[0x2E1] = 0;

    if (infoRec.hasAudio) {
        std::memcpy(rawInfo + 0xAE, &infoRec.audNumChannels, sizeof(csSDK_int32));
        const float sampleRateF = static_cast<float>(infoRec.audSampleRate);
        std::memcpy(rawInfo + 0xB2, &sampleRateF, sizeof(float));
        const csSDK_int32 sampleType = 6; // kPrAudioSampleType_32BitFloat = 6 (verified at 0x18002c483)
        std::memcpy(rawInfo + 0xB6, &sampleType, sizeof(csSDK_int32));
        csSDK_uint64 audDur = (infoRec.audDuration > 0)
                                  ? infoRec.audDuration
                                  : static_cast<csSDK_uint64>(infoRec.audSampleRate);
        for (const auto& st : session.effectiveStreams) {
            if (st.type == openmedia::MediaType::Audio && st.durationPts > 0 &&
                st.timeBase.num > 0 && st.timeBase.den > 0 && st.sampleRate > 0) {
                const double sec = static_cast<double>(st.durationPts) *
                                   static_cast<double>(st.timeBase.num) /
                                   static_cast<double>(st.timeBase.den);
                const double samples = sec * static_cast<double>(st.sampleRate);
                if (samples >= 1.0) {
                    audDur = static_cast<csSDK_uint64>(samples + 0.5);
                }
                break;
            }
        }
        std::memcpy(rawInfo + 0xBA, &audDur, sizeof(csSDK_uint64));
        const csSDK_int32 randomAccess = 1;
        std::memcpy(rawInfo + 0xC2, &randomAccess, sizeof(csSDK_int32));
    } else {
        const csSDK_int32 sampleType = 6;
        std::memcpy(rawInfo + 0xB6, &sampleType, sizeof(csSDK_int32));
        const csSDK_uint64 zero64 = 0;
        std::memcpy(rawInfo + 0xAE, &zero64, sizeof(csSDK_uint64));
        std::memcpy(rawInfo + 0xBA, &zero64, sizeof(csSDK_uint64));
        std::memcpy(rawInfo + 0xC2, &zero32, sizeof(csSDK_int32));
    }

    if (sessionKey != nullptr) {
        std::memcpy(rawInfo + 0xC6, &sessionKey, sizeof(void*));
    }

    // Ensure +0xCE (prefs) points to a valid 32-byte block ({1ULL, 0x20ULL, 0ULL, 0ULL})
    void* existingPrefs = nullptr;
    std::memcpy(&existingPrefs, rawInfo + 0xCE, sizeof(void*));
    if (existingPrefs == nullptr) {
        if (PrHostMemFuncs* mem = resolveHostMemFuncs(stdParms)) {
            if (mem->newPtr != nullptr) {
                char* prefsBlock = mem->newPtr(0x20U);
                if (prefsBlock != nullptr) {
                    std::memset(prefsBlock, 0, 0x20U);
                    const uint64_t ver = 1ULL;
                    const uint64_t len = 0x20ULL;
                    std::memcpy(prefsBlock, &ver, sizeof(uint64_t));
                    std::memcpy(prefsBlock + 8, &len, sizeof(uint64_t));
                    session.box.hostPrefsPtr = prefsBlock;
                    existingPrefs = prefsBlock;
                    std::memcpy(rawInfo + 0xCE, &existingPrefs, sizeof(void*));
                }
            }
        }
    }
}

csSDK_int32 handleImGetInfo8(imStdParms* stdParms, void* param1, void* param2) {
    if (param2 == nullptr) {
        return kImMemErr;
    }
    if (isRealAdobeHostCall(stdParms)) {
        auto* rawInfo = static_cast<uint8_t*>(param2);
        void* privKey = nullptr;
        std::memcpy(&privKey, rawInfo + 0xC6, sizeof(void*));
        auto session = findSession(privKey);
        if (!session) {
            const auto* wpathInline = reinterpret_cast<const char16_t*>(rawInfo + 0x2E2);
            std::u16string u16Path(wpathInline != nullptr ? wpathInline : u"");
            if (u16Path.empty() && param1 != nullptr) {
                const auto* wpathAccess = *reinterpret_cast<const char16_t* const*>(param1);
                if (wpathAccess != nullptr) {
                    u16Path.assign(wpathAccess);
                }
            }
            if (!u16Path.empty()) {
                session = std::make_shared<ImporterFileSession>();
                const std::string utf8Path = FileProbe::utf16ToUtf8(u16Path);
                session->mods = FilenameModifiers::parseFilenameModifiers(utf8Path);
                if (session->demuxer->openFile(session->mods.cleanPath) == CoreStatus::Ok) {
                    session->effectiveStreams = session->demuxer->getStreams();
                    FileProbe::applyModifiersToStreams(session->mods, session->effectiveStreams);
                    initSessionDecoders(*session);
                    privKey = (privKey != nullptr)
                                  ? privKey
                                  : allocateHostSessionHandle(stdParms, *session);
                } else {
                    session.reset();
                }
            }
        }
        if (!session || session->effectiveStreams.empty()) {
            return kImBadFile;
        }
        ImFileInfoRec8Ext bridgeInfo{};
        FileProbe::populateFileInfo(session->effectiveStreams, bridgeInfo);
        writeRawFileInfo8(stdParms, rawInfo, bridgeInfo, *session, privKey);
        {
            std::lock_guard<std::mutex> lock(sessionRegistryMutex());
            evictOldestSessionsLocked();
            registerSessionKeysLocked(session, privKey);
        }
        return kImNoErr;
    }

    auto* infoRec = static_cast<ImFileInfoRec8Ext*>(param2);
    std::vector<StreamInfo> streams;
    if (auto session = findSession(infoRec->privatedata)) {
        streams = session->effectiveStreams;
    } else if (param1 != nullptr) {
        const auto* accessRec = static_cast<const ImFileAccessRec8Ext*>(param1);
        const std::string utf8Path = FileProbe::utf16ToUtf8(accessRec->filepath);
        const FilenameModifiers mods = FilenameModifiers::parseFilenameModifiers(utf8Path);
        CoreDemuxer tempDemuxer;
        if (tempDemuxer.openFile(mods.cleanPath) == CoreStatus::Ok) {
            streams = tempDemuxer.getStreams();
            FileProbe::applyModifiersToStreams(mods, streams);
        }
    }
    if (streams.empty()) {
        return kImBadFile;
    }
    FileProbe::populateFileInfo(streams, *infoRec);
    MemoryGovernor::applyMemoryFlags(MemoryGovernor::queryAvailablePhysicalRamBytes(), nullptr, infoRec);
    if (infoRec->streamIdx > 0 || streams.size() > 1U) {
        return StreamTracker::advanceStreamIteration(infoRec->streamIdx, streams, *infoRec);
    }
    return kImNoErr;
}

csSDK_int32 handleImCloseFile(imStdParms* stdParms, void* param1, void* param2) {
    if (isRealAdobeHostCall(stdParms)) {
#ifdef _WIN32
        if (param1 != nullptr) {
            HANDLE hFile = *static_cast<HANDLE*>(param1);
            if (hFile != nullptr && hFile != INVALID_HANDLE_VALUE) {
                CloseHandle(hFile);
                *static_cast<HANDLE*>(param1) = INVALID_HANDLE_VALUE;
            }
        }
#endif
        if (param2 != nullptr) {
            void* handleKey = *static_cast<void**>(param2);
            auto session = findSession(handleKey);
            if (session) {
                if (PrHostMemFuncs* mem = resolveHostMemFuncs(stdParms)) {
                    if (session->box.hostHandle != nullptr && mem->disposeHandle != nullptr) {
                        if (mem->unlockHandle != nullptr) {
                            mem->unlockHandle(session->box.hostHandle);
                        }
                        mem->disposeHandle(session->box.hostHandle);
                        session->box.hostHandle = nullptr;
                    }
                }
                std::lock_guard<std::mutex> lock(sessionRegistryMutex());
                for (auto it = sessionRegistry().begin(); it != sessionRegistry().end();) {
                    if (it->second == session) {
                        it = sessionRegistry().erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            *static_cast<void**>(param2) = nullptr;
        }
        return kImNoErr;
    }

    if (param1 != nullptr) {
        std::lock_guard<std::mutex> lock(sessionRegistryMutex());
        sessionRegistry().erase(param1);
        void* deref = *static_cast<void**>(param1);
        sessionRegistry().erase(deref);
    }
    return kImNoErr;
}

csSDK_int32 handleImQuietFile(imStdParms* stdParms, void* param1, void* /*param2*/) {
    if (isRealAdobeHostCall(stdParms)) {
#ifdef _WIN32
        if (param1 != nullptr) {
            HANDLE hFile = *static_cast<HANDLE*>(param1);
            if (hFile != nullptr && hFile != INVALID_HANDLE_VALUE) {
                CloseHandle(hFile);
                *static_cast<HANDLE*>(param1) = INVALID_HANDLE_VALUE;
            }
        }
#endif
    }
    {
        std::lock_guard<std::mutex> lock(sessionRegistryMutex());
        for (auto& kv : sessionRegistry()) {
            if (kv.second && kv.second->demuxer) {
                kv.second->demuxer->quietFile();
            }
        }
    }
    return MemoryGovernor::handleQuietFile(nullptr, nullptr);
}

csSDK_int32 handleImGetPrefs8(imStdParms* stdParms, void* param1, void* param2) {
    // In real Premiere Pro x64 ABI (verified at 0x18000d727 & 0x18002f3f0 in InfluxAdobeBase.dll):
    // param2 (r8) = imGetPrefsRec*
    void* rawPtr = (param2 != nullptr) ? param2 : param1;
    if (isRealAdobeHostCall(stdParms) && rawPtr != nullptr) {
        auto* rawPrefs = static_cast<PrRawGetPrefsRec*>(rawPtr);
        if (rawPrefs->firstTime == 1 && rawPrefs->prefsLength == 0) {
            rawPrefs->prefsLength = 0x20;
            return kImNoErr;
        }
        if (rawPrefs->prefs != nullptr && rawPrefs->prefsLength >= 0x10) {
            auto* dst = static_cast<uint64_t*>(rawPrefs->prefs);
            dst[0] = 1ULL;
            dst[1] = 0x20ULL;
            if (rawPrefs->firstTime == 1) {
                return kImNoErr;
            }
        }
    }
#ifdef _WIN32
    wchar_t modulePath[MAX_PATH]{};
    HMODULE selfMod = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&handleImGetPrefs8),
                           &selfMod) &&
        GetModuleFileNameW(selfMod, modulePath, MAX_PATH) > 0) {
        std::wstring dir(modulePath);
        const size_t slash = dir.find_last_of(L"\\/");
        if (slash != std::wstring::npos) {
            const std::wstring exePath = dir.substr(0, slash + 1U) + L"OpenMediaDesktop.exe";
            if (GetFileAttributesW(exePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                ShellExecuteW(nullptr, L"open", exePath.c_str(), L"--popup", nullptr, SW_SHOWNORMAL);
            }
        }
    }
#endif
    return kImNoErr;
}

csSDK_int32 handleImPreferredFrameSize(imStdParms* /*stdParms*/, void* param1, void* /*param2*/) {
    if (param1 == nullptr) {
        return kImMemErr;
    }
    auto* rec = static_cast<PrRawPreferredFrameSizeRec*>(param1);
    if (rec->inIndex < 0 || rec->inIndex >= 5) {
        return kImOtherErr;
    }
    const csSDK_int32 scale = 1 << rec->inIndex; // 1, 2, 4, 8, 16
    rec->outWidth = (std::max)(1, rec->inWidth / scale);
    rec->outHeight = (std::max)(1, rec->inHeight / scale);
    return (rec->inIndex < 4) ? kImIterateFrameSizes : kImNoErr;
}

csSDK_int32 handleImGetTimeInfo8(imStdParms* /*stdParms*/, void* /*param1*/, void* /*param2*/) {
    return kImNoErr;
}

csSDK_int32 handleImAnalysis(imStdParms* /*stdParms*/, void* /*param1*/, void* /*param2*/) {
    return kImNoErr;
}

csSDK_int32 handleImGetMetaData(imStdParms* /*stdParms*/, void* /*param1*/, void* /*param2*/) {
    return kImUnsupported;
}

csSDK_int32 handleImSelectClipFrameDescriptor(imStdParms* /*stdParms*/, void* param1, void* param2) {
    // Verified at 0x18000d797 & 0x18002f700 in InfluxAdobeBase.dll: param2 (r8) = imClipFrameDescriptorRec*
    void* target = (param2 != nullptr) ? param2 : param1;
    if (target != nullptr) {
        auto* raw = static_cast<uint8_t*>(target);
        std::memcpy(raw + 0x24, raw + 0x08, 16);
        std::memcpy(raw + 0x30, raw + 0x14, 16);
        const csSDK_int32 zero = 0;
        std::memcpy(raw + 0x38, &zero, sizeof(csSDK_int32));
    }
    return kImNoErr;
}

csSDK_int32 handleImGetIndColorSpace(imStdParms* /*stdParms*/, void* /*param1*/, void* /*param2*/) {
    return kImBadFormatIndex;
}

csSDK_int32 renderRealHostPPix(const imStdParms* stdParms,
                               const PrRawSourceVideoRec& rawRec,
                               const RawVideoFrame& frame) noexcept {
    SPBasicSuite* basic = resolveHostSPBasicSuite(stdParms);
    if (basic == nullptr || basic->AcquireSuite == nullptr || rawRec.outFrame == nullptr) {
        return kImDecompressionError;
    }
    const PrSDKPPixCreatorSuite* creatorSuite = nullptr;
    const PrSDKPPixSuite* ppixSuite = nullptr;
    basic->AcquireSuite("Premiere PPix Creator Suite", 1,
                        reinterpret_cast<const void**>(&creatorSuite));
    basic->AcquireSuite("Premiere PPix Suite", 1,
                        reinterpret_cast<const void**>(&ppixSuite));
    if (creatorSuite == nullptr || creatorSuite->CreatePPix == nullptr ||
        ppixSuite == nullptr || ppixSuite->GetPixels == nullptr || ppixSuite->GetRowBytes == nullptr) {
        return kImDecompressionError;
    }

    int32_t outW = frame.width;
    int32_t outH = frame.height;
    csSDK_uint32 pixFmt = kPixFmtBGRA4444_8u;
    if (rawRec.inFrameFormats != nullptr && rawRec.inNumFrameFormats > 0) {
        if (rawRec.inFrameFormats[0].inWidth > 0 && rawRec.inFrameFormats[0].inHeight > 0) {
            outW = rawRec.inFrameFormats[0].inWidth;
            outH = rawRec.inFrameFormats[0].inHeight;
        }
        if (rawRec.inFrameFormats[0].inPixelFormat != 0) {
            pixFmt = rawRec.inFrameFormats[0].inPixelFormat;
        }
    }

    const prRect bounds{0, 0, outW, outH};
    PPixHand ppix = nullptr;
    if (creatorSuite->CreatePPix(&ppix, kPrHostPPixBufferAccessWriteOnly,
                                 static_cast<PrPixelFormat>(pixFmt), &bounds) != suiteError_NoError ||
        ppix == nullptr) {
        return kImMemErr;
    }

    char* dstBuf = nullptr;
    csSDK_int32 rowBytes = 0;
    if (ppixSuite->GetPixels(ppix, kPrHostPPixBufferAccessWriteOnly, &dstBuf) != suiteError_NoError ||
        dstBuf == nullptr ||
        ppixSuite->GetRowBytes(ppix, &rowBytes) != suiteError_NoError ||
        rowBytes == 0) {
        if (ppixSuite->Dispose != nullptr) {
            ppixSuite->Dispose(ppix);
        }
        return kImDecompressionError;
    }

    const int32_t tightStride = outW * 4;
    std::vector<uint8_t> topDownBgra(static_cast<size_t>(tightStride) * static_cast<size_t>(outH), 0U);
    FrameScaler scaler;
    if (scaler.scaleFrame(frame, topDownBgra.data(), tightStride,
                          PixelFormat::BGRA8, outW, outH) != CoreStatus::Ok) {
        if (!frame.planes[0].empty()) {
            const size_t copyW = (std::min<size_t>)(static_cast<size_t>(tightStride), frame.rowBytes[0]);
            const int32_t copyH = (std::min)(outH, frame.height);
            for (int32_t y = 0; y < copyH; ++y) {
                std::memcpy(topDownBgra.data() + static_cast<size_t>(y) * tightStride,
                            frame.planes[0].data() + static_cast<size_t>(y) * frame.rowBytes[0],
                            copyW);
            }
        }
    }

    // Premiere Pro 'bgra' PPix buffers are stored bottom-up (row 0 at bottom, verified at 0x18002e9af)
    const int32_t absRowBytes = std::abs(rowBytes);
    const size_t copyBytes = static_cast<size_t>((std::min)(tightStride, absRowBytes));
    for (int32_t y = 0; y < outH; ++y) {
        const uint8_t* srcRow = topDownBgra.data() + static_cast<size_t>(y) * tightStride;
        char* dstRow = dstBuf + static_cast<ptrdiff_t>(outH - 1 - y) * absRowBytes;
        std::memcpy(dstRow, srcRow, copyBytes);
    }

    *rawRec.outFrame = ppix;
    return kImNoErr;
}

csSDK_int32 handleImGetSourceVideo(imStdParms* stdParms, void* param1, void* param2) {
    if (isRealAdobeHostCall(stdParms)) {
        // Verified at 0x18000d6db & 0x18001096a in InfluxAdobeBase.dll:
        // param1 (rdx) = imFileRef, param2 (r8) = imSourceVideoRec*
        auto* rawRec = static_cast<PrRawSourceVideoRec*>(param2 != nullptr ? param2 : param1);
        if (rawRec == nullptr || rawRec->outFrame == nullptr) {
            return kImMemErr;
        }
        auto session = findSession(rawRec->privatedata);
        if (!session) session = findSession(rawRec->prefs);
        if (!session) session = findSession(rawRec->extraPrivateData);
        if (!session) session = findSession(param1, true);
        if (!session || !session->videoInitialized) {
            return kImDecompressionError;
        }

        PrTime ticksPerSec = 254016000000LL;
        if (SPBasicSuite* basic = resolveHostSPBasicSuite(stdParms)) {
            if (basic->AcquireSuite != nullptr) {
                const PrHostTimeSuiteV1* timeSuite = nullptr;
                if (basic->AcquireSuite("Premiere Time Suite", 1,
                                        reinterpret_cast<const void**>(&timeSuite)) == 0 &&
                    timeSuite != nullptr && timeSuite->GetTicksPerSecond != nullptr) {
                    PrTime hostTicks = 0;
                    if (timeSuite->GetTicksPerSecond(&hostTicks) == 0 && hostTicks > 0) {
                        ticksPerSec = hostTicks;
                    }
                }
            }
        }

        const int64_t scale = (session->vidScale > 0) ? session->vidScale : 30;
        const int64_t sampleSize = (session->vidSampleSize > 0) ? session->vidSampleSize : 1;
        const int64_t frameIdx = (rawRec->inFrameTime > 0 && ticksPerSec > 0)
                                     ? ((rawRec->inFrameTime * scale) / (ticksPerSec * sampleSize))
                                     : 0LL;

        if (frameIdx == session->lastVideoFrameIdx && session->hasLastDecodedFrame) {
            return renderRealHostPPix(stdParms, *rawRec, session->lastDecodedFrame);
        }

        if (session->lastVideoFrameIdx < 0 ||
            frameIdx < session->lastVideoFrameIdx ||
            (frameIdx - session->lastVideoFrameIdx) > 16) {
            int64_t targetPts = frameIdx;
            for (const auto& st : session->effectiveStreams) {
                if (st.index == session->videoStreamIndex &&
                    st.timeBase.num > 0 && st.timeBase.den > 0) {
                    targetPts = (frameIdx * sampleSize * st.timeBase.den) /
                                (scale * st.timeBase.num);
                    break;
                }
            }
            session->demuxer->seekToTimestamp(session->videoStreamIndex, (std::max<int64_t>)(0LL, targetPts));
            session->videoDecoder->reset();
        }
        session->lastVideoFrameIdx = frameIdx;

        RawVideoFrame decoded{};
        if (!decodeNextVideoFrameFromSession(*session, decoded)) {
            return kImDecompressionError;
        }
        return renderRealHostPPix(stdParms, *rawRec, decoded);
    }

    if (param1 == nullptr) {
        return kImMemErr;
    }
    auto* rec = static_cast<ImSourceVideoRecExt*>(param1);
    auto session = findSession(rec->privatedata);
    if (!session || !session->videoInitialized) {
        return kImUnsupported;
    }
    const int64_t targetPts = static_cast<int64_t>(rec->inFrame);
    if (session->lastVideoFrameIdx >= 0 &&
        (targetPts < session->lastVideoFrameIdx || (targetPts - session->lastVideoFrameIdx) > 30)) {
        KeyframeEntry anchor{};
        if (session->keyframeBuffer.seekExactFrame(targetPts, anchor)) {
            session->demuxer->seekToTimestamp(session->videoStreamIndex, anchor.pts);
        } else {
            session->demuxer->seekToTimestamp(session->videoStreamIndex, targetPts);
        }
        session->videoDecoder->reset();
    }
    session->lastVideoFrameIdx = targetPts;

    EncodedPacket pkt{};
    if (session->demuxer->readNextPacket(pkt) == CoreStatus::Ok &&
        pkt.streamIndex == session->videoStreamIndex) {
        if (pkt.isKeyFrame) {
            session->keyframeBuffer.addKeyframe(KeyframeEntry{pkt.pts, targetPts, 0, false});
        }
        session->videoDecoder->sendPacket(pkt);
    }
    const auto* creatorSuite = static_cast<const PrSDKPPixCreatorSuite*>(param2);
    return SourceVideoHandler::renderSourceVideo(*rec, *session->videoDecoder, creatorSuite);
}

csSDK_int32 handleImCreateAsync(imStdParms* /*stdParms*/, void* param1, void* /*param2*/) {
    return AsyncImporterEngine::handleCreateAsyncImporter(
        static_cast<ImAsyncImporterCreationRecExt*>(param1));
}

csSDK_int32 handleImImportAudio7(imStdParms* stdParms, void* param1, void* param2) {
    if (isRealAdobeHostCall(stdParms)) {
        // Verified at 0x18000d745 & 0x18002b730 in InfluxAdobeBase.dll:
        // param1 (rdx) = imFileRef, param2 (r8) = imImportAudioRec7*
        auto* rawAudio = static_cast<PrRawImportAudioRec7*>(param2 != nullptr ? param2 : param1);
        if (rawAudio == nullptr || rawAudio->buffer == nullptr) {
            return kImMemErr;
        }
        auto session = findSession(rawAudio->privatedata);
        if (!session) session = findSession(rawAudio->prefs);
        if (!session) session = findSession(param1, true);

        const uint32_t ch = (session && session->audioChannels > 0) ? session->audioChannels : 2U;
        const size_t reqSamples = (rawAudio->size > 0) ? static_cast<size_t>(rawAudio->size) : 0U;
        if (reqSamples == 0U) {
            return kImNoErr;
        }
        for (uint32_t c = 0; c < ch; ++c) {
            if (rawAudio->buffer[c] != nullptr) {
                std::memset(rawAudio->buffer[c], 0, reqSamples * sizeof(float));
            }
        }
        if (session && session->audioInitialized) {
            if (rawAudio->position >= 0 &&
                std::abs(rawAudio->position - session->lastAudioSamplePos) > 48000) {
                int64_t targetPts = rawAudio->position;
                for (const auto& st : session->effectiveStreams) {
                    if (st.index == session->audioStreamIndex &&
                        st.timeBase.num > 0 && st.timeBase.den > 0 && session->audioSampleRate > 0) {
                        targetPts = (rawAudio->position * st.timeBase.den) /
                                    (static_cast<int64_t>(session->audioSampleRate) * st.timeBase.num);
                        break;
                    }
                }
                session->demuxer->seekToTimestamp(session->audioStreamIndex, (std::max<int64_t>)(0LL, targetPts));
                session->audioDecoder->reset();
            }
            std::vector<int16_t> pcm;
            if (decodeNextAudioSamples(*session, reqSamples, ch, pcm) && !pcm.empty()) {
                const size_t availFrames = (std::min)(reqSamples, pcm.size() / ch);
                for (size_t i = 0; i < availFrames; ++i) {
                    for (uint32_t c = 0; c < ch; ++c) {
                        if (rawAudio->buffer[c] != nullptr) {
                            rawAudio->buffer[c][i] = static_cast<float>(pcm[i * ch + c]) / 32767.0f;
                        }
                    }
                }
            }
            session->lastAudioSamplePos = (rawAudio->position >= 0 ? rawAudio->position : session->lastAudioSamplePos) +
                                          static_cast<int64_t>(reqSamples);
        }
        return kImNoErr;
    }

    auto* rec = static_cast<ImImportAudioRec7Ext*>(param1);
    if (rec == nullptr) {
        return kImMemErr;
    }
    const uint32_t ch = (rec->numChannels > 0) ? static_cast<uint32_t>(rec->numChannels) : 2U;
    const size_t reqSamples = (rec->size > 0) ? static_cast<size_t>(rec->size) : 1024U;
    std::vector<int16_t> samples;
    if (auto session = findSession(rec->privatedata)) {
        if (decodeNextAudioSamples(*session, reqSamples, ch, samples)) {
            return AudioExtractor::importAudio7FaultTolerant(rec, samples, ch, false);
        }
    }
    const std::vector<int16_t> silence(reqSamples * ch, 0);
    return AudioExtractor::importAudio7FaultTolerant(rec, silence, ch, false);
}

csSDK_int32 handleImGetPeakAudio(imStdParms* /*stdParms*/, void* param1, void* /*param2*/) {
    auto* rec = static_cast<ImPeakAudioRecExt*>(param1);
    if (rec == nullptr) {
        return kImMemErr;
    }
    const std::size_t count = (rec->numSampleFrames > 0) ? rec->numSampleFrames : 256U;
    const std::size_t decim = (rec->decimationFactor > 0) ? rec->decimationFactor : 256U;
    std::vector<float> samples(count, 0.0f);
    if (auto session = findSession(rec->privatedata)) {
        std::vector<int16_t> pcm;
        if (decodeNextAudioSamples(*session, count, 1U, pcm) && !pcm.empty()) {
            const size_t n = (std::min)(count, pcm.size());
            for (size_t i = 0; i < n; ++i) {
                samples[i] = static_cast<float>(pcm[i]) / 32767.0f;
            }
        }
    }
    rec->minPeaks.resize(1U);
    rec->maxPeaks.resize(1U);
    return PeakWaveformGenerator::generatePeaks(samples, decim, rec->minPeaks[0], rec->maxPeaks[0]);
}

csSDK_int32 handleImAudioLayout(imStdParms* /*stdParms*/, void* param1, void* /*param2*/) {
    auto* rec = static_cast<ImAudioChannelLayoutRecExt*>(param1);
    if (rec == nullptr) {
        return kImMemErr;
    }
    return MultiChannelRouter::resolveLayout(rec->numChannels == 0 ? 2U : rec->numChannels, *rec);
}

csSDK_int32 handleImIndPixelFormat(imStdParms* stdParms, void* param1, void* param2) {
    const auto idx = static_cast<int32_t>(reinterpret_cast<std::intptr_t>(param1));
    if (param2 == nullptr) {
        return kImMemErr;
    }
    if (isRealAdobeHostCall(stdParms)) {
        // Verified at 0x18002cfe0 in InfluxAdobeBase.dll:
        // param1 (rdx) = index, param2 (r8) + 0x08 = outPixelFormat ('bgra' = 0x61726762)
        if (idx == 0) {
            const csSDK_uint32 bgra = kPixFmtBGRA4444_8u; // 'bgra'
            std::memcpy(static_cast<uint8_t*>(param2) + 0x08, &bgra, sizeof(csSDK_uint32));
            return kImNoErr;
        }
        return kImBadFormatIndex;
    }
    auto* rec = static_cast<ImIndPixelFormatRecExt*>(param2);
    return PixelFormat8uEngine::getSupported8uFormat(idx, false, rec->outPixelFormat);
}

// Dispatch table supporting BOTH official Premiere Pro SDK selectors AND legacy mock numbers
constexpr std::array<SelectorHandlerEntry, 31> kSelectorTable = {{
    {kSelInit, &handleImInit},                                    // 0
    {kSelShutdown, &handleImShutdown},                            // 1
    {kSelQuietFile, &handleImQuietFile},                          // 8 (official)
    {kSelCloseFile, &handleImCloseFile},                          // 9 (official)
    {kSelAnalysis, &handleImAnalysis},                            // 12 (official)
    {kSelGetIndFormat, &handleImGetIndFormat},                    // 14 (official)
    {kSelGetMetaData, &handleImGetMetaData},                      // 23 (official -> returns 3 imUnsupported)
    {kSelSetMetaData, &handleImAnalysis},                         // 24 (official -> returns 0 imNoErr)
    {kSelImportAudio7, &handleImImportAudio7},                    // 25 (official)
    {kSelGetIndPixelFormat, &handleImIndPixelFormat},             // 27 (official)
    {kSelGetSupports7, &handleImGetSupports7},                    // 28 (official)
    {kSelGetSupports8, &handleImGetSupports8},                    // 36 (official)
    {kSelGetInfo8, &handleImGetInfo8},                            // 37 (official)
    {kSelGetPrefs8, &handleImGetPrefs8},                          // 38 (official)
    {kSelOpenFile8, &handleImOpenFile8},                          // 39 (official)
    {kSelGetPreferredFrameSize, &handleImPreferredFrameSize},     // 45 (official)
    {kSelCreateAsyncImporter, &handleImCreateAsync},              // 46 (official)
    {kSelGetSourceVideo, &handleImGetSourceVideo},                // 47 (official)
    {kSelGetPeakAudio, &handleImGetPeakAudio},                    // 49 (official)
    {kSelGetTimeInfo8, &handleImGetTimeInfo8},                    // 52 (official)
    {kSelSetTimeInfo8, &handleImGetTimeInfo8},                    // 53 (official)
    {kSelGetAudioChannelLayout, &handleImAudioLayout},            // 64 (official)
    {kSelSelectClipFrameDescriptor, &handleImSelectClipFrameDescriptor}, // 65 (official)
    {kSelGetIndColorSpace, &handleImGetIndColorSpace},            // 73 (official)
    // Legacy low-number aliases for backward compatibility with unit tests:
    {2, &handleImGetInfo8},
    {3, &handleImOpenFile8},
    {4, &handleImCloseFile},
    {5, &handleImGetIndFormat},
    {6, &handleImGetSourceVideo},
    {10, &handleImGetPeakAudio},
    {11, &handleImAudioLayout},
}};

} // namespace

csSDK_int32 handleImInitWithPriority(imStdParms* stdParms,
                                     void* param1,
                                     void* /*param2*/,
                                     bool isHighPriority) {
    if (param1 == nullptr) {
        return kImMemErr;
    }

    if (isRealAdobeHostCall(stdParms)) {
        // Verified at 0x1800292e0 in InfluxAdobeBase.dll:
        // +0x24 = canOpen (1), +0x28 = canGetPrefs8 (1), +0x34 = priority (101 for HP, 100 for standard)
        auto* rawInfo = static_cast<PrRawImportInfoRec*>(param1);
        rawInfo->privatedata = 0;
        rawInfo->canOpen = 1;
        rawInfo->canGetPrefs8 = 1;
        rawInfo->setupOnDblClk = 0;
        rawInfo->priority = isHighPriority ? 101 : 100;
        rawInfo->noFile = 0;
        rawInfo->avoidAudioConform = 1;
        return kImNoErr;
    }

    auto* info = static_cast<ImImportInfoRecExt*>(param1);
    info->canOpen = kPrTrueVal;
    info->hasSetup = kPrTrueVal;
    info->priority = isHighPriority ? kHighImporterPriority : kStandardImporterPriority;
    info->avoidAudioConform = kPrTrueVal;
    info->canProvidePeakAudio = kPrTrueVal;
    return kImNoErr;
}

csSDK_int32 handleImInit(imStdParms* stdParms, void* param1, void* param2) {
    return handleImInitWithPriority(stdParms, param1, param2, false);
}

csSDK_int32 handleImGetIndFormat(imStdParms* stdParms, void* param1, void* param2) {
    const auto index = static_cast<csSDK_int32>(reinterpret_cast<std::intptr_t>(param1));
    if (index < 0 || static_cast<std::size_t>(index) >= kSupportedFormats.size()) {
        return kImBadFormatIndex;
    }
    if (param2 == nullptr) {
        return kImMemErr;
    }

    const auto& fmt = kSupportedFormats[static_cast<std::size_t>(index)];
    if (isRealAdobeHostCall(stdParms)) {
        auto* rawRec = static_cast<PrRawIndFormatRec*>(param2);
        std::memset(rawRec, 0, sizeof(PrRawIndFormatRec));
        rawRec->filetype = fmt.distinctFourCC;
        rawRec->flags = 0x10; // xfCanOpen | xfIsMovie
        rawRec->canAutoOpen = 0;
        std::snprintf(rawRec->formatName, sizeof(rawRec->formatName), "%s", fmt.formatName);
        std::snprintf(rawRec->formatShortName, sizeof(rawRec->formatShortName), "%s", fmt.formatShortName);
        const size_t extLen = std::strlen(fmt.extension);
        if (extLen + 2U < sizeof(rawRec->platformExtension)) {
            std::memcpy(rawRec->platformExtension, fmt.extension, extLen);
            rawRec->platformExtension[extLen] = '\0';
            rawRec->platformExtension[extLen + 1U] = '\0'; // double-null terminated list
        }
        return kImNoErr;
    }

    auto* rec = static_cast<ImIndFormatRecExt*>(param2);
    rec->filetype = kOpenMediaImporterFourCC;
    std::snprintf(rec->formatName, sizeof(rec->formatName), "%s", fmt.formatName);
    std::snprintf(rec->formatShortName, sizeof(rec->formatShortName), "%s", fmt.formatShortName);
    std::snprintf(rec->extension, sizeof(rec->extension), "%s", fmt.extension);
    return kImNoErr;
}

csSDK_int32 dispatchImporterSelector(csSDK_int32 selector,
                                     imStdParms* stdParms,
                                     void* param1,
                                     void* param2) {
    for (const auto& entry : kSelectorTable) {
        if (entry.selector == selector && entry.handler != nullptr) {
            return entry.handler(stdParms, param1, param2);
        }
    }
    return kImUnsupported;
}

} // namespace openmedia::importer

extern "C" DllExport PREMPLUGENTRY xOpenMediaImportEntry(csSDK_int32 selector,
                                                         imStdParms* stdParms,
                                                         void* param1,
                                                         void* param2,
                                                         bool isHighPriority) {
    if (selector == openmedia::importer::kSelInit) {
        return openmedia::importer::handleImInitWithPriority(stdParms, param1, param2, isHighPriority);
    }
    return openmedia::importer::dispatchImporterSelector(selector, stdParms, param1, param2);
}

extern "C" DllExport PREMPLUGENTRY xImportEntry(csSDK_int32 selector,
                                                imStdParms* stdParms,
                                                void* param1,
                                                void* param2) {
#ifdef OPENMEDIA_IMPORTER_HIGH_PRIORITY
    constexpr bool kHighPriority = true;
#else
    constexpr bool kHighPriority = false;
#endif
    return xOpenMediaImportEntry(selector, stdParms, param1, param2, kHighPriority);
}
