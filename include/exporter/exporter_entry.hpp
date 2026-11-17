#pragma once
/// @file exporter_entry.hpp
/// @brief Adobe MediaCore Exporter entry point (xSDKExport / xExportEntry) and multi-container iteration (v3.0.2 ABI).

#include "exporter/export_param_builder.hpp"
#include "exporter/exporter_types.hpp"

#ifndef DllExport
#ifdef _WIN32
#define DllExport __declspec(dllexport)
#else
#define DllExport __attribute__((visibility("default")))
#endif
#endif

/// @brief Official PrSDKExport.h return codes for real Adobe Premiere Pro / Media Encoder hosts.
inline constexpr csSDK_int32 kPrHostExportReturn_ErrNone = 0;
inline constexpr csSDK_int32 kPrHostExportReturn_IterateExporter = 16;     // 0x10 in official PrSDKExport.h
inline constexpr csSDK_int32 kPrHostExportReturn_IterateExporterDone = 17; // 0x11 in official PrSDKExport.h

/// @brief Selector constant for exSelQueryStillSequence when not provided by minimal mock SDK.
inline constexpr csSDK_int32 kExSelQueryStillSequence = 10;

#pragma pack(push, 4)
struct PrRawExporterInfoRec {
    csSDK_uint32 exporterPluginID;
    csSDK_uint32 fileType;
    char16_t fileTypeName[256];
    char16_t fileTypeDefaultExtension[256];
    csSDK_uint32 classID;
    csSDK_int32 exportReqIndex;
    csSDK_int32 wantsNoProgressBar;
    csSDK_int32 hideInUI;
    csSDK_int32 doesNotSupportAudioOnly;
    csSDK_int32 canExportVideo;
    csSDK_int32 canExportAudio;
    csSDK_int32 singleFrameOnly;
    csSDK_uint32 interfaceVersion;
    csSDK_int32 isCacheable;
    csSDK_int32 canConformToMatchParams;
};

struct PrRawParamSummaryRec {
    csSDK_uint32 exporterPluginID;
    csSDK_int32 exportAudio;
    csSDK_int32 exportVideo;
    csSDK_int32 reserved;
    char16_t summary1[256];
    char16_t summary2[256];
    char16_t summary3[256];
};

struct PrRawQueryExportFileExtensionRec {
    csSDK_uint32 exporterPluginID;
    csSDK_uint32 reserved;
    csSDK_uint32 fileType;
    csSDK_uint32 reserved2;
    char16_t outFileExtension[256];
};

struct PrRawQueryStillSequenceRec {
    csSDK_uint32 exporterPluginID;
    csSDK_int32 outExportAsStillSequence;
    PrTime outExportFrameRate;
};
#pragma pack(pop)

extern "C" {
DllExport PREMPLUGENTRY xSDKExport(csSDK_int32 selector,
                                   exportStdParms* stdParms,
                                   void* param1,
                                   void* param2);

DllExport PREMPLUGENTRY xExportEntry(csSDK_int32 selector,
                                     exportStdParms* stdParms,
                                     void* param1,
                                     void* param2);
}

namespace openmedia::exporter {

class ExporterEntry {
public:
    static csSDK_int32 handleStartup(ExStartupInfoRecExt* info) noexcept;
    static csSDK_int32 handleRealHostStartup(PrRawExporterInfoRec* rawInfo) noexcept;
    static csSDK_int32 handleBeginInstance(void* instanceData) noexcept;
    static csSDK_int32 handleEndInstance(void* instanceData) noexcept;
    static csSDK_int32 handleQueryStillSequence(prBool* outIsStillSeq) noexcept;
    static csSDK_int32 dispatch(csSDK_int32 selector,
                                exportStdParms* stdParms,
                                void* param1,
                                void* param2) noexcept;
    static std::vector<std::string> buildPopupLaunchArgs(uint64_t ownerHwnd);
    static bool exchangePopupSettings(std::istream& in,
                                      std::ostream& out,
                                      const PopupIpcMessage& request,
                                      PopupIpcMessage& outResponse);
};

} // namespace openmedia::exporter
