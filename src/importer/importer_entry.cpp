/// @file importer_entry.cpp
/// @brief Implementation of xOpenMediaImportEntry, xImportEntry, full selector dispatch, and per-file session state.

#include "importer/importer_entry.hpp"
#include "importer/async_importer.hpp"
#include "importer/audio_extractor.hpp"
#include "importer/file_probe.hpp"
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

#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace openmedia::importer {
namespace {

struct SupportedFormatDesc {
    csSDK_uint32 filetype;
    const char* formatName;
    const char* formatShortName;
    const char* extension;
};

constexpr std::array<SupportedFormatDesc, 7> kSupportedFormats = {{
    {kOpenMediaImporterFourCC, "Matroska Video (OpenMedia)", "Matroska", "mkv"},
    {kOpenMediaImporterFourCC, "WebM Video (OpenMedia)", "WebM", "webm"},
    {kOpenMediaImporterFourCC, "Ogg Media (OpenMedia)", "Ogg", "ogg"},
    {kOpenMediaImporterFourCC, "Flash Video (OpenMedia)", "FLV", "flv"},
    {kOpenMediaImporterFourCC, "AVI Video (OpenMedia)", "AVI", "avi"},
    {kOpenMediaImporterFourCC, "MPEG-4 Video (OpenMedia)", "MP4", "mp4"},
    {kOpenMediaImporterFourCC, "QuickTime Movie (OpenMedia)", "QuickTime", "mov"},
}};

struct ImporterFileSession {
    std::unique_ptr<CoreDemuxer> demuxer = std::make_unique<CoreDemuxer>();
    std::unique_ptr<CoreVideoDecoder> videoDecoder = std::make_unique<CoreVideoDecoder>();
    std::unique_ptr<CoreAudioDecoder> audioDecoder = std::make_unique<CoreAudioDecoder>();
    bool videoInitialized = false;
    bool audioInitialized = false;
    uint32_t audioChannels = 2U;
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
    return (it != sessionRegistry().end()) ? it->second : nullptr;
}

void initSessionDecoders(ImporterFileSession& session) {
    for (const auto& st : session.demuxer->getStreams()) {
        if (st.type == MediaType::Video && !session.videoInitialized) {
            session.videoInitialized = (session.videoDecoder->initialize(st) == CoreStatus::Ok);
        } else if (st.type == MediaType::Audio && !session.audioInitialized) {
            session.audioChannels = (st.channels > 0) ? static_cast<uint32_t>(st.channels) : 2U;
            const int32_t rate = (st.sampleRate > 0) ? st.sampleRate : 48000;
            session.audioInitialized = (session.audioDecoder->initialize(st, rate) == CoreStatus::Ok);
        }
    }
}

csSDK_int32 handleImShutdown(imStdParms* /*stdParms*/, void* /*param1*/, void* /*param2*/) {
    std::lock_guard<std::mutex> lock(sessionRegistryMutex());
    sessionRegistry().clear();
    return kImNoErr;
}

csSDK_int32 handleImOpenFile8(imStdParms* /*stdParms*/, void* param1, void* /*param2*/) {
    auto* openRec = static_cast<ImOpenFileRec8Ext*>(param1);
    if (openRec == nullptr) {
        return kImMemErr;
    }
    auto session = std::make_shared<ImporterFileSession>();
    const csSDK_int32 status = FileProbe::openAndProbe(*openRec, *session->demuxer);
    if (status != kImNoErr) {
        return status;
    }
    initSessionDecoders(*session);
    MemoryGovernor::applyMemoryFlags(MemoryGovernor::queryAvailablePhysicalRamBytes(), openRec, nullptr);
    void* handleKey = session.get();
    openRec->privatedata = handleKey;
    std::lock_guard<std::mutex> lock(sessionRegistryMutex());
    sessionRegistry()[handleKey] = std::move(session);
    return kImNoErr;
}

csSDK_int32 handleImGetInfo8(imStdParms* /*stdParms*/, void* param1, void* param2) {
    auto* infoRec = static_cast<ImFileInfoRec8Ext*>(param2);
    if (infoRec == nullptr) {
        return kImMemErr;
    }
    std::vector<StreamInfo> streams;
    if (auto session = findSession(infoRec->privatedata)) {
        streams = session->demuxer->getStreams();
    } else if (param1 != nullptr) {
        const auto* accessRec = static_cast<const ImFileAccessRec8Ext*>(param1);
        CoreDemuxer tempDemuxer;
        if (tempDemuxer.openFile(FileProbe::utf16ToUtf8(accessRec->filepath)) == CoreStatus::Ok) {
            streams = tempDemuxer.getStreams();
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

csSDK_int32 handleImGetPrefs8(imStdParms* /*stdParms*/, void* /*param1*/, void* /*param2*/) {
    return kImNoErr;
}

csSDK_int32 handleImGetSourceVideo(imStdParms* /*stdParms*/, void* param1, void* param2) {
    auto* rec = static_cast<ImSourceVideoRecExt*>(param1);
    if (rec == nullptr) {
        return kImMemErr;
    }
    auto session = findSession(rec->privatedata);
    if (!session || !session->videoInitialized) {
        return kImUnsupported;
    }
    EncodedPacket pkt{};
    if (session->demuxer->readNextPacket(pkt) == CoreStatus::Ok) {
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
    const std::vector<int16_t> silence(static_cast<std::size_t>(rec->size) * ch, 0);
    return AudioExtractor::importAudio7FaultTolerant(rec, silence, ch, false);
}

csSDK_int32 handleImGetPeakAudio(imStdParms* /*stdParms*/, void* param1, void* /*param2*/) {
    auto* rec = static_cast<ImPeakAudioRecExt*>(param1);
    if (rec == nullptr) {
        return kImMemErr;
    }
    const std::size_t count = (rec->numSampleFrames > 0) ? rec->numSampleFrames : 256U;
    const std::size_t decim = (rec->decimationFactor > 0) ? rec->decimationFactor : 256U;
    const std::vector<float> samples(count, 0.0f);
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

constexpr std::array<SelectorHandlerEntry, 14> kSelectorTable = {{
    {kSelInit, &handleImInit},
    {kSelShutdown, &handleImShutdown},
    {kSelGetInfo8, &handleImGetInfo8},
    {kSelOpenFile8, &handleImOpenFile8},
    {kSelCloseFile, &handleImCloseFile},
    {kSelGetIndFormat, &handleImGetIndFormat},
    {kSelGetSourceVideo, &handleImGetSourceVideo},
    {kSelGetIndPixelFormat, &handleImIndPixelFormat},
    {kSelImportAudio7, &handleImImportAudio7},
    {kSelGetPeakAudio, &handleImGetPeakAudio},
    {kSelGetAudioChannelLayout, &handleImAudioLayout},
    {kSelCreateAsyncImporter, &handleImCreateAsync},
    {kSelQuietFile, &handleImQuietFile},
    {kSelGetPrefs8, &handleImGetPrefs8},
}};

} // namespace

csSDK_int32 handleImInitWithPriority(imStdParms* /*stdParms*/,
                                     void* param1,
                                     void* /*param2*/,
                                     bool isHighPriority) {
    if (param1 == nullptr) {
        return kImMemErr;
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

csSDK_int32 handleImGetIndFormat(imStdParms* /*stdParms*/, void* param1, void* param2) {
    const auto index = static_cast<csSDK_int32>(reinterpret_cast<std::intptr_t>(param1));
    if (index < 0 || static_cast<std::size_t>(index) >= kSupportedFormats.size()) {
        return kImBadFormatIndex;
    }
    if (param2 == nullptr) {
        return kImMemErr;
    }

    const auto& fmt = kSupportedFormats[static_cast<std::size_t>(index)];
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
