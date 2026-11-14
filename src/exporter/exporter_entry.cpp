#include "exporter/exporter_entry.hpp"

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
csSDK_int32 onBeginInstance(csSDK_int32, exportStdParms*, void* param1, void*) noexcept {
    return ExporterEntry::handleBeginInstance(param1);
}
csSDK_int32 onEndInstance(csSDK_int32, exportStdParms*, void* param1, void*) noexcept {
    return ExporterEntry::handleEndInstance(param1);
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
        {exSelBeginInstance, &onBeginInstance},
        {exSelEndInstance, &onEndInstance},
        {kExSelQueryStillSequence, &onQueryStill},
    };
    for (const auto& entry : kTable) {
        if (entry.selector == selector) return entry.handler(selector, stdParms, param1, param2);
    }
    return kExNoErr;
}
} // namespace openmedia::exporter

extern "C" DllExport csSDK_int32 PREMPLUGENTRY xSDKExport(csSDK_int32 selector, exportStdParms* stdParms, void* param1, void* param2) {
    return openmedia::exporter::ExporterEntry::dispatch(selector, stdParms, param1, param2);
}
