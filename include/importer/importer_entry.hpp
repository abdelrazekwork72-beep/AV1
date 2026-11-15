#pragma once
/// @file importer_entry.hpp
/// @brief Adobe MediaCore Importer entry point and dual-PRM selector dispatch declarations.

#include "PrSDKTypes.h"
#include "PrSDKPixelFormat.h"

#ifndef PREMPLUGENTRY
/// @brief Calling/return convention for MediaCore plugin entry points when not defined by SDK.
#define PREMPLUGENTRY csSDK_int32
#endif

#include "importer/importer_types.hpp"

/// @brief Shared MediaCore Importer dispatcher supporting standard and high-priority PRMs.
/// @param selector MediaCore importer selector code.
/// @param stdParms Pointer to standard Adobe MediaCore callback suite parameters.
/// @param param1 Selector-specific first parameter.
/// @param param2 Selector-specific second parameter.
/// @param isHighPriority True when invoked from OpenMedia_ImporterHP.prm.
/// @return MediaCore importer status code.
extern "C" DllExport PREMPLUGENTRY xOpenMediaImportEntry(csSDK_int32 selector,
                                                          imStdParms* stdParms,
                                                          void* param1,
                                                          void* param2,
                                                          bool isHighPriority);

/// @brief Main Adobe MediaCore Importer plugin entry point exported by OpenMedia_Importer(.prm/HP.prm).
extern "C" DllExport PREMPLUGENTRY xImportEntry(csSDK_int32 selector,
                                                imStdParms* stdParms,
                                                void* param1,
                                                void* param2);

namespace openmedia::importer {

/// @brief Standard priority for OpenMedia_Importer.prm.
inline constexpr csSDK_int32 kStandardImporterPriority = 100;

/// @brief Elevated priority for OpenMedia_ImporterHP.prm.
inline constexpr csSDK_int32 kHighImporterPriority = 200;

/// @brief Handles the imInit selector to advertise importer capabilities and priority.
csSDK_int32 handleImInit(imStdParms* stdParms, void* param1, void* param2);

/// @brief Handles imInit with explicit high-priority flag.
csSDK_int32 handleImInitWithPriority(imStdParms* stdParms,
                                     void* param1,
                                     void* param2,
                                     bool isHighPriority);

/// @brief Handles the imGetIndFormat selector to register supported container formats by index.
csSDK_int32 handleImGetIndFormat(imStdParms* stdParms, void* param1, void* param2);

/// @brief Dispatches an importer selector through the static SelectorHandlerEntry table.
csSDK_int32 dispatchImporterSelector(csSDK_int32 selector,
                                     imStdParms* stdParms,
                                     void* param1,
                                     void* param2);

} // namespace openmedia::importer
