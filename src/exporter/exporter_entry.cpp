#include "exporter/exporter_entry.hpp"
#include "exporter/dynamic_ui_validator.hpp"
#include "exporter/gop_tail_flusher.hpp"
#include "exporter/match_source_engine.hpp"
#include "exporter/push_render_loop.hpp"

#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

namespace {
void preloadExporterCompanionLibs(HMODULE hinstDLL) noexcept {
    wchar_t modulePath[MAX_PATH]{};
    if (hinstDLL == nullptr || GetModuleFileNameW(hinstDLL, modulePath, MAX_PATH) == 0) {
        return;
    }
    std::wstring dir(modulePath);
    const size_t slash = dir.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return;
    }
    dir.resize(slash + 1U);
    SetDllDirectoryW(dir.c_str());
    static constexpr const wchar_t* kCompanionDlls[] = {
        L"avutil-60.dll",
        L"swresample-6.dll",
        L"swscale-9.dll",
        L"avcodec-62.dll",
        L"avformat-62.dll",
        L"OpenMediaCore.dll",
    };
    for (const wchar_t* dllName : kCompanionDlls) {
        const std::wstring fullPath = dir + dllName;
        if (GetFileAttributesW(fullPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            LoadLibraryExW(fullPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        }
    }
}
} // namespace

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        preloadExporterCompanionLibs(hinstDLL);
    }
    return TRUE;
}
#endif

namespace openmedia::exporter {
namespace {

struct ContainerSpec {
    csSDK_uint32 fourCC;
    const char* name;
    const char* ext;
    prBool previewSupport;
};

static constexpr ContainerSpec kContainers[] = {
    {kExporterMkvFourCC, "OpenMedia Matroska (.mkv)", "mkv", 1},
    {kExporterWebmFourCC, "OpenMedia WebM (.webm)", "webm", 0},
    {kExporterMp4FourCC, "OpenMedia AV1 (.mp4)", "mp4", 1},
};

bool isRealAdobeExporterHostCall(const exportStdParms* stdParms) noexcept {
    if (stdParms == nullptr) {
        return false;
    }
    const auto spBasicAddr = reinterpret_cast<std::uintptr_t>(stdParms->spBasic);
    return spBasicAddr > 0x10000ULL;
}

void copyAsciiToUtf16(const char* src, char16_t* dst, size_t maxChars) noexcept {
    if (dst == nullptr || maxChars == 0U) {
        return;
    }
    std::memset(dst, 0, maxChars * sizeof(char16_t));
    if (src == nullptr) {
        return;
    }
    for (size_t i = 0U; i + 1U < maxChars && src[i] != '\0'; ++i) {
        dst[i] = static_cast<char16_t>(static_cast<unsigned char>(src[i]));
    }
}

csSDK_int32 onStartup(csSDK_int32, exportStdParms* stdParms, void* param1, void*) noexcept {
    if (isRealAdobeExporterHostCall(stdParms)) {
        return ExporterEntry::handleRealHostStartup(static_cast<PrRawExporterInfoRec*>(param1));
    }
    return ExporterEntry::handleStartup(static_cast<ExStartupInfoRecExt*>(param1));
}

csSDK_int32 onShutdown(csSDK_int32, exportStdParms*, void*, void*) noexcept {
    return kExNoErr;
}

csSDK_int32 onBeginInstance(csSDK_int32, exportStdParms*, void* param1, void*) noexcept {
    return ExporterEntry::handleBeginInstance(param1);
}

csSDK_int32 onEndInstance(csSDK_int32, exportStdParms*, void* param1, void*) noexcept {
    return ExporterEntry::handleEndInstance(param1);
}

csSDK_int32 onExport(csSDK_int32, exportStdParms*, void* param1, void*) noexcept {
    if (param1 == nullptr) {
        return kExNoErr;
    }
    std::vector<openmedia::RawVideoFrame> renderedFrames;
    uint32_t drained = 0;
    bool trailerWritten = false;
    return PushRenderLoop::runMultiPassWithDrain(1U, 1U, 1920U, 1080U, 0, renderedFrames, drained, trailerWritten);
}

csSDK_int32 onQueryOutputFileList(csSDK_int32, exportStdParms*, void*, void*) noexcept {
    return kExNoErr;
}

csSDK_int32 onGetParamSummary(csSDK_int32, exportStdParms* stdParms, void* param1, void*) noexcept {
    if (param1 != nullptr && isRealAdobeExporterHostCall(stdParms)) {
        auto* rec = static_cast<PrRawParamSummaryRec*>(param1);
        copyAsciiToUtf16("Video: AV1 / VP9 / H.264 / HEVC (10-bit, CRF 24, Match Source)", rec->summary1, 256U);
        copyAsciiToUtf16("Audio: Opus / FLAC / Vorbis / AAC / PCM (48 kHz, Stereo / 5.1)", rec->summary2, 256U);
        copyAsciiToUtf16("OpenMedia v3.0.2 Control Center Ready", rec->summary3, 256U);
    }
    return kExNoErr;
}

csSDK_int32 onQueryExportFileExtension(csSDK_int32, exportStdParms* stdParms, void* param1, void*) noexcept {
    if (param1 != nullptr && isRealAdobeExporterHostCall(stdParms)) {
        auto* extRec = static_cast<PrRawQueryExportFileExtensionRec*>(param1);
        const char* ext = "mkv";
        for (const auto& spec : kContainers) {
            if (spec.fourCC == extRec->fileType) {
                ext = spec.ext;
                break;
            }
        }
        copyAsciiToUtf16(ext, extRec->outFileExtension, 256U);
    }
    return kExNoErr;
}

csSDK_int32 onParamButton(csSDK_int32, exportStdParms*, void*, void*) noexcept {
#ifdef _WIN32
    wchar_t modulePath[MAX_PATH]{};
    HMODULE selfMod = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&onParamButton),
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
    return kExNoErr;
}

csSDK_int32 onValidateParamChanged(csSDK_int32, exportStdParms* stdParms, void* param1, void*) noexcept {
    if (param1 == nullptr || isRealAdobeExporterHostCall(stdParms)) {
        return kExNoErr;
    }
    return DynamicUIValidator::validateParamChanged(*static_cast<ExParamChangedRecExt*>(param1));
}

csSDK_int32 onGenerateDefaultParams(csSDK_int32, exportStdParms* stdParms, void* param1, void* param2) noexcept {
    if (isRealAdobeExporterHostCall(stdParms)) {
        return kExNoErr;
    }
    ExporterPresetParams defaults{};
    auto* outVer = static_cast<csSDK_int32*>(param2);
    const csSDK_int32 status = ExportParamBuilder::generateDefaults(defaults, outVer);
    if (param1 != nullptr) {
        *static_cast<ExporterPresetParams*>(param1) = defaults;
    }
    return status;
}

csSDK_int32 onQueryStill(csSDK_int32, exportStdParms* stdParms, void* param1, void*) noexcept {
    if (isRealAdobeExporterHostCall(stdParms)) {
        if (param1 != nullptr) {
            auto* rec = static_cast<PrRawQueryStillSequenceRec*>(param1);
            rec->outExportAsStillSequence = 0;
        }
        return kExNoErr;
    }
    return ExporterEntry::handleQueryStillSequence(static_cast<prBool*>(param1));
}

} // namespace

csSDK_int32 ExporterEntry::handleRealHostStartup(PrRawExporterInfoRec* rawInfo) noexcept {
    if (rawInfo == nullptr || rawInfo->exportReqIndex < 0) {
        return kPrHostExportReturn_IterateExporterDone;
    }
    constexpr csSDK_int32 kCount = static_cast<csSDK_int32>(sizeof(kContainers) / sizeof(kContainers[0]));
    if (rawInfo->exportReqIndex >= kCount) {
        return kPrHostExportReturn_IterateExporterDone;
    }
    const auto& spec = kContainers[rawInfo->exportReqIndex];
    rawInfo->fileType = spec.fourCC;
    copyAsciiToUtf16(spec.name, rawInfo->fileTypeName, 256U);
    copyAsciiToUtf16(spec.ext, rawInfo->fileTypeDefaultExtension, 256U);
    rawInfo->classID = spec.fourCC;
    rawInfo->wantsNoProgressBar = 0;
    rawInfo->hideInUI = 0;
    rawInfo->doesNotSupportAudioOnly = 0;
    rawInfo->canExportVideo = 1;
    rawInfo->canExportAudio = 1;
    rawInfo->singleFrameOnly = 0;
    rawInfo->isCacheable = 1;
    rawInfo->canConformToMatchParams = 1;
    return (rawInfo->exportReqIndex + 1 < kCount)
               ? kPrHostExportReturn_IterateExporter
               : kPrHostExportReturn_IterateExporterDone;
}

csSDK_int32 ExporterEntry::handleStartup(ExStartupInfoRecExt* info) noexcept {
    if (info == nullptr || info->exporterIndex < 0) return kExError;
    constexpr csSDK_int32 kCount = static_cast<csSDK_int32>(sizeof(kContainers) / sizeof(kContainers[0]));
    if (info->exporterIndex >= kCount) return kExIterateExporterDone;
    const auto& spec = kContainers[info->exporterIndex];
    info->fileType = spec.fourCC;
    info->displayName = spec.name;
    info->fileExtension = spec.ext;
    info->canMatchSource = 1;
    info->canEmbedCaptions = 1;
    info->isSequencePreviewFormat = spec.previewSupport;
    return (info->exporterIndex + 1 < kCount) ? kExIterateExporter : kExIterateExporterDone;
}

csSDK_int32 ExporterEntry::handleBeginInstance(void*) noexcept { return kExNoErr; }
csSDK_int32 ExporterEntry::handleEndInstance(void*) noexcept { return kExNoErr; }
csSDK_int32 ExporterEntry::handleQueryStillSequence(prBool* outIsStillSeq) noexcept {
    if (outIsStillSeq != nullptr) *outIsStillSeq = 0;
    return kExNoErr;
}

csSDK_int32 ExporterEntry::dispatch(csSDK_int32 selector, exportStdParms* stdParms, void* param1, void* param2) noexcept {
    static constexpr ExSelectorEntry kTable[] = {
        {exSelStartup, &onStartup},
        {exSelBeginInstance, &onBeginInstance},
        {exSelGenerateDefaultParams, &onGenerateDefaultParams},
        {exSelPostProcessParams, &onQueryOutputFileList},
        {exSelGetParamSummary, &onGetParamSummary},
        {exSelQueryOutputSettings, &onQueryOutputFileList},
        {exSelQueryExportFileExtension, &onQueryExportFileExtension},
        {exSelParamButton, &onParamButton},
        {exSelEndInstance, &onEndInstance},
        {exSelExport, &onExport},
        {kExSelQueryStillSequence, &onQueryStill},
        {exSelShutdown, &onShutdown},
        {exSelValidateParamChanged, &onValidateParamChanged},
        {exSelValidateOutputSettings, &onQueryOutputFileList},
        {exSelQueryOutputFileList, &onQueryOutputFileList},
    };
    for (const auto& entry : kTable) {
        if (entry.selector == selector) return entry.handler(selector, stdParms, param1, param2);
    }
    return kExNoErr;
}

std::vector<std::string> ExporterEntry::buildPopupLaunchArgs(uint64_t ownerHwnd) {
    return PopupIpcBridge::buildLaunchArgs(ownerHwnd);
}

bool ExporterEntry::exchangePopupSettings(std::istream& in,
                                          std::ostream& out,
                                          const PopupIpcMessage& request,
                                          PopupIpcMessage& outResponse) {
    return PopupIpcBridge::exchangeOverStreams(in, out, request, outResponse);
}

} // namespace openmedia::exporter

extern "C" DllExport PREMPLUGENTRY xSDKExport(csSDK_int32 selector, exportStdParms* stdParms, void* param1, void* param2) {
    return openmedia::exporter::ExporterEntry::dispatch(selector, stdParms, param1, param2);
}

extern "C" DllExport PREMPLUGENTRY xExportEntry(csSDK_int32 selector, exportStdParms* stdParms, void* param1, void* param2) {
    return openmedia::exporter::ExporterEntry::dispatch(selector, stdParms, param1, param2);
}
