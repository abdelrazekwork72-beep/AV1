#pragma once
/// @file exporter_types.hpp
/// @brief Shared clean-room Adobe MediaCore Exporter SDK types, constants, and invariants.

#include "PrSDKEntry.h"
#include "PrSDKExport.h"
#include "PrSDKExportParamSuite.h"
#include "PrSDKExportProgressSuite.h"
#include "PrSDKExportStdParamSuite.h"
#include "PrSDKExporterUtilitySuite.h"
#include "PrSDKCaptioningSuite.h"
#include "PrSDKErrorSuite.h"
#include "PrSDKPixelFormat.h"
#include "PrSDKSequenceAudioSuite.h"
#include "PrSDKSequenceRenderSuite.h"
#include "PrSDKTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace openmedia::exporter {

/// @brief Exporter parameter tree version invariant.
inline constexpr csSDK_int32 kOpenMediaExporterParamsVersion = 1001;

/// @brief Registered container FourCC identifiers.
inline constexpr csSDK_uint32 kExporterMkvFourCC = 0x4F4D4D4BU; // 'OMMK'
inline constexpr csSDK_uint32 kExporterWebmFourCC = 0x4F4D574DU; // 'OMWM'
inline constexpr csSDK_uint32 kExporterMp4FourCC = 0x4F4D4D50U; // 'OMMP'

/// @brief Exporter return codes aligned with Adobe PrSDKExport.h.
inline constexpr csSDK_int32 kExNoErr = 0;
inline constexpr csSDK_int32 kExIterateExporter = 1001;
inline constexpr csSDK_int32 kExIterateExporterDone = 1002;
inline constexpr csSDK_int32 kExAbort = 1003;
inline constexpr csSDK_int32 kExError = -1;
inline constexpr csSDK_int32 kExSuspended = 2001;

/// @brief Extended startup info for multi-container exporter registration.
struct ExStartupInfoRecExt {
    csSDK_int32 exporterIndex = 0;
    csSDK_uint32 fileType = 0;
    std::string displayName;
    std::string fileExtension;
    prBool canMatchSource = 0;
    prBool canEmbedCaptions = 0;
    prBool isSequencePreviewFormat = 0;
};

/// @brief Extended parameter validation record.
struct ExParamChangedRecExt {
    std::string changedParamId;
    bool hardwareEnabled = false;
    int32_t width = 1920;
    int32_t height = 1080;
    int32_t bitDepth = 8;
    int32_t chromaSubsampling = 420;
    prBool rebuildAllParams = 0;
};

/// @brief Static dispatch table entry for xSDKExport.
using ExSelectorFn = csSDK_int32 (*)(csSDK_int32 selector, exportStdParms* stdParms, void* param1, void* param2);
struct ExSelectorEntry {
    csSDK_int32 selector;
    ExSelectorFn handler;
};

} // namespace openmedia::exporter
