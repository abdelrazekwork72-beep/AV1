#pragma once
/// @file exporter_entry.hpp
/// @brief Adobe MediaCore Exporter entry point (xSDKExport) and multi-container iteration.

#include "exporter/exporter_types.hpp"

#ifndef DllExport
#define DllExport
#endif

/// @brief Selector constant for exSelQueryStillSequence when not provided by minimal mock SDK.
inline constexpr csSDK_int32 kExSelQueryStillSequence = 10;

extern "C" {
/// @brief Primary Adobe Premiere Pro / Media Encoder exporter entry point.
DllExport csSDK_int32 PREMPLUGENTRY xSDKExport(csSDK_int32 selector,
                                                 exportStdParms* stdParms,
                                                 void* param1,
                                                 void* param2);
}

namespace openmedia::exporter {

/// @brief Manages multi-container exporter registration (MKV, WebM, AV1 MP4) and selector dispatch.
class ExporterEntry {
public:
    /// @brief Handle exSelStartup iterating MKV (0), WebM (1), and AV1 MP4 (2).
    static csSDK_int32 handleStartup(ExStartupInfoRecExt* info) noexcept;
    /// @brief Handle exSelBeginInstance.
    static csSDK_int32 handleBeginInstance(void* instanceData) noexcept;
    /// @brief Handle exSelEndInstance.
    static csSDK_int32 handleEndInstance(void* instanceData) noexcept;
    /// @brief Handle exSelQueryStillSequence.
    static csSDK_int32 handleQueryStillSequence(prBool* outIsStillSeq) noexcept;
    /// @brief Dispatch selector via static lookup table (CC <= 4).
    static csSDK_int32 dispatch(csSDK_int32 selector,
                                exportStdParms* stdParms,
                                void* param1,
                                void* param2) noexcept;
};

} // namespace openmedia::exporter
