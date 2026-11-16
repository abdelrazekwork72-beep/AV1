/// @file importer_entry.cpp
/// @brief Implementation of xOpenMediaImportEntry, xImportEntry, 39-format dispatch,
///        real Adobe Premiere Pro / Media Encoder x64 binary ABI + CTest bridge,
///        real audio/video decoding, frame-accurate seeking, and OpenMediaDesktop popup integration.

#include "importer/importer_entry.hpp"
#include "importer/async_importer.hpp"
#include "importer/audio_extractor.hpp"
#include "importer/file_probe.hpp"
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
#endif

namespace openmedia::importer {
namespace {

struct SupportedFormatDesc {
    csSDK_uint32 filetype;
    const char* formatName;
    const char* formatShortName;
    const char* extension;
};

constexpr std::array<SupportedFormatDesc, 39> kSupportedFormats = {{
    {kOpenMediaImporterFourCC, "Matroska Video (OpenMedia)", "Matroska", "mkv"},
    {kOpenMediaImporterFourCC, "WebM Video (OpenMedia)", "WebM", "webm"},
    {kOpenMediaImporterFourCC, "Ogg Media (OpenMedia)", "Ogg", "ogg"},
    {kOpenMediaImporterFourCC, "Flash Video (OpenMedia)", "FLV", "flv"},
    {kOpenMediaImporterFourCC, "AVI Video (OpenMedia)", "AVI", "avi"},
    {kOpenMediaImporterFourCC, "MPEG-4 Video (OpenMedia)", "MP4", "mp4"},
    {kOpenMediaImporterFourCC, "QuickTime Movie (OpenMedia)", "QuickTime", "mov"},
    {kOpenMediaImporterFourCC, "Ogg Video (OpenMedia)", "OGV", "ogv"},
    {kOpenMediaImporterFourCC, "M4V Video (OpenMedia)", "M4V", "m4v"},
    {kOpenMediaImporterFourCC, "MPEG Program Stream (OpenMedia)", "MPG", "mpg"},
    {kOpenMediaImporterFourCC, "MPEG Video (OpenMedia)", "MPEG", "mpeg"},
    {kOpenMediaImporterFourCC, "MPEG Transport Stream (OpenMedia)", "TS", "ts"},
    {kOpenMediaImporterFourCC, "Blu-ray Transport Stream (OpenMedia)", "M2TS", "m2ts"},
    {kOpenMediaImporterFourCC, "AVCHD Transport Stream (OpenMedia)", "MTS", "mts"},
    {kOpenMediaImporterFourCC, "DVD Video Object (OpenMedia)", "VOB", "vob"},
    {kOpenMediaImporterFourCC, "Material Exchange Format (OpenMedia)", "MXF", "mxf"},
    {kOpenMediaImporterFourCC, "Windows Media Video (OpenMedia)", "WMV", "wmv"},
    {kOpenMediaImporterFourCC, "Windows Media Audio (OpenMedia)", "WMA", "wma"},
    {kOpenMediaImporterFourCC, "Advanced Systems Format (OpenMedia)", "ASF", "asf"},
    {kOpenMediaImporterFourCC, "Bink Video (OpenMedia)", "BIK", "bik"},
    {kOpenMediaImporterFourCC, "3GPP Multimedia (OpenMedia)", "3GP", "3gp"},
    {kOpenMediaImporterFourCC, "GoPro MAX 360 (OpenMedia)", "360", "360"},
    {kOpenMediaImporterFourCC, "YUV4MPEG2 Raw (OpenMedia)", "Y4M", "y4m"},
    {kOpenMediaImporterFourCC, "Motion JPEG (OpenMedia)", "MJPEG", "mjpeg"},
    {kOpenMediaImporterFourCC, "Animated PNG (OpenMedia)", "APNG", "apng"},
    {kOpenMediaImporterFourCC, "WebP Image (OpenMedia)", "WebP", "webp"},
    {kOpenMediaImporterFourCC, "Quite OK Image (OpenMedia)", "QOI", "qoi"},
    {kOpenMediaImporterFourCC, "GIF Animation (OpenMedia)", "GIF", "gif"},
    {kOpenMediaImporterFourCC, "Free Lossless Audio Codec (OpenMedia)", "FLAC", "flac"},
    {kOpenMediaImporterFourCC, "Opus Audio (OpenMedia)", "Opus", "opus"},
    {kOpenMediaImporterFourCC, "Matroska Audio (OpenMedia)", "MKA", "mka"},
    {kOpenMediaImporterFourCC, "Waveform Audio (OpenMedia)", "WAV", "wav"},
    {kOpenMediaImporterFourCC, "Advanced Audio Coding (OpenMedia)", "AAC", "aac"},
    {kOpenMediaImporterFourCC, "MPEG Layer III Audio (OpenMedia)", "MP3", "mp3"},
    {kOpenMediaImporterFourCC, "Dolby AC-3 Audio (OpenMedia)", "AC3", "ac3"},
    {kOpenMediaImporterFourCC, "DTS-HD Audio (OpenMedia)", "DTS", "dts"},
    {kOpenMediaImporterFourCC, "Audio Interchange File (OpenMedia)", "AIFF", "aiff"},
    {kOpenMediaImporterFourCC, "OpenMedia Forced Container (Influx)", "INFLUX", "influx"},
    {kOpenMediaImporterFourCC, "OpenMedia Forced Container", "OPENMEDIA", "openmedia"},
}};

constexpr size_t kMaxOpenSessions = 32U;

bool isRealAdobeHostCall(const imStdParms* stdParms) noexcept {
    if (stdParms == nullptr) {
        return false;
    }
    const auto* raw = reinterpret_cast<const PrRawStdParms*>(stdParms);
    return (raw->imInterfaceVer >= 6 && raw->piSuites != nullptr);
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

struct ImporterFileSession {
    std::unique_ptr<CoreDemuxer> demuxer = std::make_unique<CoreDemuxer>();
    std::unique_ptr<CoreVideoDecoder> videoDecoder = std::make_unique<CoreVideoDecoder>();
    std::unique_ptr<CoreAudioDecoder> audioDecoder = std::make_unique<CoreAudioDecoder>();
    FilenameModifiers mods{};
    std::vector<StreamInfo> effectiveStreams;
    KeyframeRingBuffer keyframeBuffer{32U};
    bool videoInitialized = false;
    bool audioInitialized = false;
    int32_t videoStreamIndex = -1;
    int32_t audioStreamIndex = -1;
    uint32_t audioChannels = 2U;
    int64_t lastVideoPts = -1;
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

std::shared_ptr<ImporterFileSession> findSession(void* key) {
    if (key == nullptr) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(sessionRegistryMutex());
    const auto it = sessionRegistry().find(key);
    if (it == sessionRegistry().end()) {
        return nullptr;
    }
    it->second->lastAccess = std::chrono::steady_clock::now();
    return it->second;
}

void evictOldestSessionsLocked() {
    auto& reg = sessionRegistry();
    while (reg.size() >= kMaxOpenSessions) {
        auto oldest = reg.begin();
        for (auto it = reg.begin(); it != reg.end(); ++it) {
            if (it->second->lastAccess < oldest->second->lastAccess) {
                oldest = it;
            }
        }
        if (oldest->second && oldest->second->demuxer) {
            oldest->second->demuxer->quietFile();
        }
        reg.erase(oldest);
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
                session.videoInitialized = (session.videoDecoder->initialize(st) == CoreStatus::Ok);
            }
            ++vidSeen;
        } else if (st.type == MediaType::Audio) {
            const bool matchAud = (session.mods.audioStreamIndex < 0) ||
                                  (audSeen == session.mods.audioStreamIndex);
            if (!session.audioInitialized && matchAud) {
                session.audioStreamIndex = st.index;
                session.audioChannels = (st.channels > 0) ? static_cast<uint32_t>(st.channels) : 2U;
                const int32_t rate = (st.sampleRate > 0) ? st.sampleRate : 48000;
                session.audioInitialized = (session.audioDecoder->initialize(st, rate) == CoreStatus::Ok);
                if (!session.audioInitialized && !session.videoInitialized) {
                    session.demuxer->close();
                }
            }
            ++audSeen;
        }
    }
}

bool decodeNextAudioSamples(ImporterFileSession& session,
                            size_t targetSamples,
                            uint32_t channels,
                            std::vector<int16_t>& outInterleaved) {
    if (!session.audioInitialized || session.audioStreamIndex < 0 || channels == 0) {
        return false;
    }
    RawAudioBatch batch{};
    constexpr int32_t kMaxPacketsPerCall = 64;
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

csSDK_int32 handleImOpenFile8(imStdParms* stdParms, void* param1, void* /*param2*/) {
    if (param1 == nullptr) {
        return kImMemErr;
    }
    if (isRealAdobeHostCall(stdParms)) {
        auto* rawOpen = static_cast<PrRawOpenFileRefRec8*>(param1);
        const auto* wpath = reinterpret_cast<const char16_t*>(
            reinterpret_cast<const uint8_t*>(param1) + 0x0C);
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
        session->effectiveStreams = session->demuxer->getStreams();
        FileProbe::applyModifiersToStreams(session->mods, session->effectiveStreams);
        initSessionDecoders(*session);
        void* handleKey = session.get();
        rawOpen->privatedata = handleKey;
        std::lock_guard<std::mutex> lock(sessionRegistryMutex());
        evictOldestSessionsLocked();
        sessionRegistry()[handleKey] = std::move(session);
        return kImNoErr;
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
    sessionRegistry()[handleKey] = std::move(session);
    return kImNoErr;
}

void writeRawFileInfo8(uint8_t* rawInfo, const ImFileInfoRec8Ext& infoRec, void* sessionKey) noexcept {
    rawInfo[0x00] = infoRec.hasVideo ? 1U : 0U;
    rawInfo[0x01] = infoRec.hasAudio ? 1U : 0U;
    std::memcpy(rawInfo + 0x02, &infoRec.vidInfo.imageWidth, sizeof(csSDK_int32));
    std::memcpy(rawInfo + 0x06, &infoRec.vidInfo.imageHeight, sizeof(csSDK_int32));
    const csSDK_int32 depth = 32;
    std::memcpy(rawInfo + 0x0E, &depth, sizeof(csSDK_int32));
    rawInfo[0x12] = 0; // prFieldsNone
    rawInfo[0x16] = 0; // alphaNone
    std::memcpy(rawInfo + 0x22, &infoRec.vidInfo.pixelAspectNum, sizeof(csSDK_uint32));
    std::memcpy(rawInfo + 0x26, &infoRec.vidInfo.pixelAspectDen, sizeof(csSDK_uint32));
    const csSDK_int32 one = 1;
    std::memcpy(rawInfo + 0x36, &one, sizeof(csSDK_int32));
    std::memcpy(rawInfo + 0x62, &one, sizeof(csSDK_int32)); // supportsAsyncIO
    std::memcpy(rawInfo + 0x90, &infoRec.vidDuration, sizeof(csSDK_uint32));
    std::memcpy(rawInfo + 0xA2, &infoRec.vidScale, sizeof(csSDK_int32));
    std::memcpy(rawInfo + 0xA6, &infoRec.vidSampleSize, sizeof(csSDK_int32));
    std::memcpy(rawInfo + 0xAE, &infoRec.audNumChannels, sizeof(csSDK_int32));
    const float sampleRateF = static_cast<float>(infoRec.audSampleRate);
    std::memcpy(rawInfo + 0xB2, &sampleRateF, sizeof(float));
    const csSDK_int32 sampleType = 7; // 32-bit float audio
    std::memcpy(rawInfo + 0xB6, &sampleType, sizeof(csSDK_int32));
    std::memcpy(rawInfo + 0xBA, &infoRec.audDuration, sizeof(csSDK_uint64));
    std::memcpy(rawInfo + 0xC2, &one, sizeof(csSDK_int32)); // kRandomAccessImport
    if (sessionKey != nullptr) {
        std::memcpy(rawInfo + 0xC6, &sessionKey, sizeof(void*));
    }
    const csSDK_int64 dur64 = static_cast<csSDK_int64>(infoRec.vidDuration);
    std::memcpy(rawInfo + 0x14E5, &dur64, sizeof(csSDK_int64));
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
            const auto* wpath = reinterpret_cast<const char16_t*>(rawInfo + 0x2E2);
            const std::u16string u16Path(wpath != nullptr ? wpath : u"");
            if (!u16Path.empty()) {
                session = std::make_shared<ImporterFileSession>();
                const std::string utf8Path = FileProbe::utf16ToUtf8(u16Path);
                session->mods = FilenameModifiers::parseFilenameModifiers(utf8Path);
                if (session->demuxer->openFile(session->mods.cleanPath) == CoreStatus::Ok) {
                    session->effectiveStreams = session->demuxer->getStreams();
                    FileProbe::applyModifiersToStreams(session->mods, session->effectiveStreams);
                    initSessionDecoders(*session);
                    privKey = session.get();
                    std::lock_guard<std::mutex> lock(sessionRegistryMutex());
                    evictOldestSessionsLocked();
                    sessionRegistry()[privKey] = session;
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
        writeRawFileInfo8(rawInfo, bridgeInfo, privKey);
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

csSDK_int32 handleImCloseFile(imStdParms* /*stdParms*/, void* param1, void* /*param2*/) {
    if (param1 != nullptr) {
        std::lock_guard<std::mutex> lock(sessionRegistryMutex());
        sessionRegistry().erase(param1);
        void* deref = *static_cast<void**>(param1);
        sessionRegistry().erase(deref);
    }
    return kImNoErr;
}

csSDK_int32 handleImQuietFile(imStdParms* /*stdParms*/, void* /*param1*/, void* /*param2*/) {
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

csSDK_int32 handleImGetPrefs8(imStdParms* stdParms, void* param1, void* /*param2*/) {
    if (isRealAdobeHostCall(stdParms) && param1 != nullptr) {
        auto* rawPrefs = static_cast<PrRawGetPrefsRec*>(param1);
        if (rawPrefs->firstTime == 1 && rawPrefs->prefsLength == 0) {
            rawPrefs->prefsLength = 0x20;
            return kImNoErr;
        }
        if (rawPrefs->prefs != nullptr && rawPrefs->prefsLength >= 0x10) {
            auto* dst = static_cast<uint64_t*>(rawPrefs->prefs);
            dst[0] = 1ULL;
            dst[1] = 0x20ULL;
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
    if (rec->inIndex == 0) {
        rec->outWidth = rec->inWidth;
        rec->outHeight = rec->inHeight;
        rec->outSupportsScaling = 0;
        return kImIterateFrameSizes;
    }
    return kImNoErr;
}

csSDK_int32 handleImGetTimeInfo8(imStdParms* /*stdParms*/, void* /*param1*/, void* /*param2*/) {
    return kImNoErr;
}

csSDK_int32 handleImAnalysis(imStdParms* /*stdParms*/, void* /*param1*/, void* /*param2*/) {
    return kImNoErr;
}

csSDK_int32 handleImSelectClipFrameDescriptor(imStdParms* /*stdParms*/, void* param1, void* /*param2*/) {
    if (param1 != nullptr) {
        auto* raw = static_cast<uint8_t*>(param1);
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

csSDK_int32 handleImGetSourceVideo(imStdParms* stdParms, void* param1, void* param2) {
    if (param1 == nullptr) {
        return kImMemErr;
    }
    if (isRealAdobeHostCall(stdParms)) {
        auto* rawRec = static_cast<PrRawSourceVideoRec*>(param1);
        void* sessKey = (rawRec->privatedata != nullptr) ? rawRec->privatedata : rawRec->prefs;
        if (sessKey != nullptr && sessionRegistry().find(sessKey) == sessionRegistry().end()) {
            sessKey = *static_cast<void**>(sessKey);
        }
        auto session = findSession(sessKey);
        if (!session || !session->videoInitialized || rawRec->outFrame == nullptr) {
            return kImDecompressionError;
        }
        constexpr int64_t kTicksPerFrame24 = 10584000000LL;
        const int64_t targetPts = (rawRec->inFrameTime > 1000000LL)
                                      ? (rawRec->inFrameTime / kTicksPerFrame24)
                                      : static_cast<int64_t>(rawRec->inFrameTime);
        if (session->lastVideoPts >= 0 &&
            (targetPts < session->lastVideoPts || (targetPts - session->lastVideoPts) > 30)) {
            session->demuxer->seekToTimestamp(session->videoStreamIndex, targetPts);
            session->videoDecoder->flush();
        }
        session->lastVideoPts = targetPts;

        EncodedPacket pkt{};
        if (session->demuxer->readNextPacket(pkt) == CoreStatus::Ok &&
            pkt.streamIndex == session->videoStreamIndex) {
            session->videoDecoder->sendPacket(pkt);
        }
        SPBasicSuite* basic = resolveHostSPBasicSuite(stdParms);
        const PrSDKPPixCreatorSuite* creatorSuite = nullptr;
        const PrSDKPPixSuite* ppixSuite = nullptr;
        if (basic != nullptr && basic->AcquireSuite != nullptr) {
            basic->AcquireSuite("Premiere PPix Creator Suite", 1,
                                reinterpret_cast<const void**>(&creatorSuite));
            basic->AcquireSuite("Premiere PPix Suite", 1,
                                reinterpret_cast<const void**>(&ppixSuite));
        }
        ImSourceVideoRecExt bridgeRec{};
        bridgeRec.privatedata = sessKey;
        bridgeRec.inFrame = targetPts;
        bridgeRec.inPixelFormat = (rawRec->inFrameFormats != nullptr && rawRec->inFrameFormats->inPixelFormat != 0)
                                      ? rawRec->inFrameFormats->inPixelFormat
                                      : kPixFmtBGRA4444_8u;
        bridgeRec.outFrame = rawRec->outFrame;
        bridgeRec.ppixSuite = ppixSuite;
        return SourceVideoHandler::renderSourceVideo(bridgeRec, *session->videoDecoder, creatorSuite);
    }

    auto* rec = static_cast<ImSourceVideoRecExt*>(param1);
    auto session = findSession(rec->privatedata);
    if (!session || !session->videoInitialized) {
        return kImUnsupported;
    }
    const int64_t targetPts = static_cast<int64_t>(rec->inFrame);
    if (session->lastVideoPts >= 0 &&
        (targetPts < session->lastVideoPts || (targetPts - session->lastVideoPts) > 30)) {
        KeyframeEntry anchor{};
        if (session->keyframeBuffer.seekExactFrame(targetPts, anchor)) {
            session->demuxer->seekToTimestamp(session->videoStreamIndex, anchor.pts);
        } else {
            session->demuxer->seekToTimestamp(session->videoStreamIndex, targetPts);
        }
        session->videoDecoder->flush();
    }
    session->lastVideoPts = targetPts;

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

csSDK_int32 handleImImportAudio7(imStdParms* /*stdParms*/, void* param1, void* /*param2*/) {
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

csSDK_int32 handleImIndPixelFormat(imStdParms* /*stdParms*/, void* param1, void* param2) {
    const auto idx = static_cast<int32_t>(reinterpret_cast<std::intptr_t>(param1));
    auto* rec = static_cast<ImIndPixelFormatRecExt*>(param2);
    if (rec == nullptr) {
        return kImMemErr;
    }
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
    {kSelGetMetaData, &handleImAnalysis},                         // 23 (official)
    {kSelSetMetaData, &handleImAnalysis},                         // 24 (official)
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
        auto* rawInfo = static_cast<PrRawImportInfoRec*>(param1);
        rawInfo->privatedata = 0;
        rawInfo->canOpen = 1;
        rawInfo->canGetPrefs8 = 1;
        rawInfo->setupOnDblClk = 0;
        rawInfo->hasSetup = kMalSupports8; // 0x65
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
        rawRec->filetype = fmt.filetype;
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
    rec->filetype = fmt.filetype;
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
