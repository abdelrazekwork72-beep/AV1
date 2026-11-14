#pragma once
/// @file importer_entry.hpp
/// @brief Adobe MediaCore Importer entry point and selector dispatch declarations for OpenMedia Importer.

#include "PrSDKTypes.h"
#include "PrSDKPixelFormat.h"

#ifndef PREMPLUGENTRY
/// @brief Calling/return convention for MediaCore plugin entry points when not defined by SDK.
#define PREMPLUGENTRY csSDK_int32
#endif


#include "importer/importer_types.hpp"

/// @brief Main Adobe MediaCore Importer plugin entry point exported by OpenMedia_Importer.prm.
/// @param selector MediaCore importer selector code (e.g., kSelInit, kSelGetIndFormat).
/// @param stdParms Pointer to standard Adobe MediaCore callback suite parameters.
/// @param param1 Selector-specific first parameter.
/// @param param2 Selector-specific second parameter.
/// @return MediaCore importer status code (kImNoErr on success, kImUnsupported if unhandled).
extern "C" DllExport PREMPLUGENTRY xImportEntry(csSDK_int32 selector,
                                                imStdParms* stdParms,
                                                void* param1,
                                                void* param2);

namespace openmedia::importer {

/// @brief Handles the imInit selector to advertise importer capabilities and priority.
/// @param stdParms Standard MediaCore parameters (may be nullptr in unit tests).
/// @param param1 Pointer to ImImportInfoRecExt structure to populate.
/// @param param2 Unused selector parameter.
/// @return kImNoErr on success, or kImMemErr if param1 is nullptr.
csSDK_int32 handleImInit(imStdParms* stdParms, void* param1, void* param2);

/// @brief Handles the imGetIndFormat selector to register supported container formats by index.
/// @param stdParms Standard MediaCore parameters (may be nullptr in unit tests).
/// @param param1 Zero-based format index cast to void*.
/// @param param2 Pointer to ImIndFormatRecExt structure to populate.
/// @return kImNoErr if index is in [0, 6], kImBadFormatIndex if out of range, or kImMemErr if param2 is nullptr.
csSDK_int32 handleImGetIndFormat(imStdParms* stdParms, void* param1, void* param2);

/// @brief Dispatches an importer selector through the static SelectorHandlerEntry table.
/// @param selector MediaCore importer selector code.
/// @param stdParms Pointer to standard Adobe MediaCore parameters.
/// @param param1 Selector-specific first parameter.
/// @param param2 Selector-specific second parameter.
/// @return Result code from the matched handler, or kImUnsupported if no handler matches.
csSDK_int32 dispatchImporterSelector(csSDK_int32 selector,
                                     imStdParms* stdParms,
                                     void* param1,
                                     void* param2);

} // namespace openmedia::importer
