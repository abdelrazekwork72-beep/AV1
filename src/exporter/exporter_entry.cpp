#include "exporter/exporter_entry.hpp"
#include "exporter/dynamic_ui_validator.hpp"
#include "exporter/gop_tail_flusher.hpp"
#include "exporter/match_source_engine.hpp"
#include "exporter/push_render_loop.hpp"

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
csSDK_int32 onStartup(csSDK_int32, exportStdParms*, void* param1, void*) noexcept {
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
csSDK_int32 onValidateParamChanged(csSDK_int32, exportStdParms*, void* param1, void*) noexcept {
    if (param1 == nullptr) {
        return kExNoErr;
    }
    return DynamicUIValidator::validateParamChanged(*static_cast<ExParamChangedRecExt*>(param1));
}
csSDK_int32 onGenerateDefaultParams(csSDK_int32, exportStdParms*, void* param1, void* param2) noexcept {
    ExporterPresetParams defaults{};
    auto* outVer = static_cast<csSDK_int32*>(param2);
    const csSDK_int32 status = ExportParamBuilder::generateDefaults(defaults, outVer);
    if (param1 != nullptr) {
        *static_cast<ExporterPresetParams*>(param1) = defaults;
    }
    return status;
}
csSDK_int32 onQueryStill(csSDK_int32, exportStdParms*, void* param1, void*) noexcept {
    return ExporterEntry::handleQueryStillSequence(static_cast<prBool*>(param1));
}
} // namespace

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
        {exSelShutdown, &onShutdown},
        {exSelBeginInstance, &onBeginInstance},
        {exSelEndInstance, &onEndInstance},
        {exSelExport, &onExport},
        {exSelQueryOutputFileList, &onQueryOutputFileList},
        {exSelValidateParamChanged, &onValidateParamChanged},
        {exSelGenerateDefaultParams, &onGenerateDefaultParams},
        {kExSelQueryStillSequence, &onQueryStill},
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
