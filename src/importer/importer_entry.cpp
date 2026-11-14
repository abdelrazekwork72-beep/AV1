/// @file importer_entry.cpp
/// @brief Implementation of xImportEntry, static selector dispatch table, imInit, and imGetIndFormat.

#include "importer/importer_entry.hpp"
#include "importer/async_importer.hpp"
#include "importer/memory_governor.hpp"
#include "importer/multi_channel_router.hpp"
#include "importer/pixel_format_8u.hpp"

#include <array>
#include <cstdint>
#include <cstdio>

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

csSDK_int32 handleImShutdown(imStdParms* /*stdParms*/, void* /*param1*/, void* /*param2*/) {
    return kImNoErr;
}

csSDK_int32 handleImQuietFile(imStdParms* /*stdParms*/, void* /*param1*/, void* /*param2*/) {
    return MemoryGovernor::handleQuietFile(nullptr, nullptr);
}

csSDK_int32 handleImCreateAsync(imStdParms* /*stdParms*/, void* param1, void* /*param2*/) {
    return AsyncImporterEngine::handleCreateAsyncImporter(
        static_cast<ImAsyncImporterCreationRecExt*>(param1));
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

constexpr std::array<SelectorHandlerEntry, 7> kSelectorTable = {{
    {kSelInit, &handleImInit},
    {kSelShutdown, &handleImShutdown},
    {kSelGetIndFormat, &handleImGetIndFormat},
    {kSelQuietFile, &handleImQuietFile},
    {kSelCreateAsyncImporter, &handleImCreateAsync},
    {kSelGetAudioChannelLayout, &handleImAudioLayout},
    {kSelGetIndPixelFormat, &handleImIndPixelFormat},
}};

} // namespace

csSDK_int32 handleImInit(imStdParms* /*stdParms*/, void* param1, void* /*param2*/) {
    if (param1 == nullptr) {
        return kImMemErr;
    }

    auto* info = static_cast<ImImportInfoRecExt*>(param1);
    info->canOpen = kPrTrueVal;
    info->hasSetup = kPrTrueVal;
    info->priority = 100;
    info->avoidAudioConform = kPrTrueVal;
    info->canProvidePeakAudio = kPrTrueVal;
    return kImNoErr;
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

extern "C" DllExport PREMPLUGENTRY xImportEntry(csSDK_int32 selector,
                                                imStdParms* stdParms,
                                                void* param1,
                                                void* param2) {
    return openmedia::importer::dispatchImporterSelector(selector, stdParms, param1, param2);
}
