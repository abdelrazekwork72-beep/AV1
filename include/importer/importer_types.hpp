#pragma once
/// @file importer_types.hpp
/// @brief Shared clean-room Adobe MediaCore Importer bridge types, selectors, and constants for Module 2.

#include "openmedia/core_types.hpp"

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "PrSDKTypes.h"
#include "PrSDKPixelFormat.h"
#include "PrSDKImport.h"
#include "PrSDKPPixSuite.h"
#include "PrSDKPPixCacheSuite.h"
#include "PrSDKAudioSuite.h"

#ifndef DllExport
#ifdef _WIN32
#define DllExport __declspec(dllexport)
#else
#define DllExport __attribute__((visibility("default")))
#endif
#endif

#ifndef PREMPLUGENTRY
#define PREMPLUGENTRY
#endif

namespace openmedia::importer {

/// @brief Unique FourCC for OpenMedia Importer ('OMIP'), strictly avoiding After Effects reserved 'MOov' (0x4D4F6F76).
inline constexpr uint32_t kOpenMediaImporterFourCC = 0x4F4D4950U;
static_assert(kOpenMediaImporterFourCC != 0x4D4F6F76U,
              "FourCC must not collide with After Effects reserved 0x4D4F6F76");

/// @brief Extended MediaCore Importer selectors beyond minimal mock_sdk PrSDKImport.h.
inline constexpr csSDK_int32 kSelInit = imInit;
inline constexpr csSDK_int32 kSelShutdown = imShutdown;
inline constexpr csSDK_int32 kSelGetInfo8 = imGetInfo8;
inline constexpr csSDK_int32 kSelOpenFile8 = imOpenFile8;
inline constexpr csSDK_int32 kSelCloseFile = imCloseFile;
inline constexpr csSDK_int32 kSelGetIndFormat = imGetIndFormat;
inline constexpr csSDK_int32 kSelGetSourceVideo = imGetSourceVideo;
inline constexpr csSDK_int32 kSelGetIndPixelFormat = 8;
inline constexpr csSDK_int32 kSelImportAudio7 = 9;
inline constexpr csSDK_int32 kSelGetPeakAudio = 10;
inline constexpr csSDK_int32 kSelGetAudioChannelLayout = 11;
inline constexpr csSDK_int32 kSelCreateAsyncImporter = 12;
inline constexpr csSDK_int32 kSelQuietFile = 13;
inline constexpr csSDK_int32 kSelGetPrefs8 = 14;

/// @brief Extended MediaCore Importer return codes.
inline constexpr csSDK_int32 kImNoErr = malNoError;
inline constexpr csSDK_int32 kImUnsupported = imUnsupported;
inline constexpr csSDK_int32 kImMemErr = imMemErr;
inline constexpr csSDK_int32 kImBadFile = 1001;
inline constexpr csSDK_int32 kImBadFormatIndex = 1002;
inline constexpr csSDK_int32 kImIterateStreams = 1003;
inline constexpr csSDK_int32 kImBadStreamIndex = 1004;

/// @brief Adobe boolean and access-mode constants.
inline constexpr prBool kPrTrueVal = 1;
inline constexpr prBool kPrFalseVal = 0;
inline constexpr csSDK_uint32 kRandomAccessImportFlag = 0x00000001U;
inline constexpr csSDK_int32 kPPixBufferAccessWriteOnly = 1;
inline constexpr csSDK_int32 kPPixBufferAccessReadOnly = 0;

/// @brief Extended pixel format constants for 10/12-bit HDR and alpha negotiation.
inline constexpr csSDK_uint32 kPixFmtBGRA4444_8u = PrPixelFormat_BGRA_4444_8u;
inline constexpr csSDK_uint32 kPixFmtVUYA4444_8u = PrPixelFormat_VUYA_4444_8u;
inline constexpr csSDK_uint32 kPixFmtYUV420Planar_8u_709 = PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u;
inline constexpr csSDK_uint32 kPixFmtRGB444_10u = PrPixelFormat_RGB_444_10u;
inline constexpr csSDK_uint32 kPixFmtUYVY422_32f = 0x55593332U;
inline constexpr csSDK_uint32 kPixFmtBGRA4444_32f_Linear = 0x42473332U;

/// @brief Standard PrAudioChannelLabel identifiers (1..32 discrete channels).
enum class PrAudioChannelLabelId : uint32_t {
    Left = 1,
    Right = 2,
    Center = 3,
    LFE = 4,
    LeftSurround = 5,
    RightSurround = 6,
    LeftBack = 7,
    RightBack = 8,
    DiscreteBase = 100
};

/// @brief Extended imImportInfoRec structure populated during imInit.
struct ImImportInfoRecExt {
    prBool canOpen = 0;
    prBool hasSetup = 0;
    prBool canSave = 0;
    prBool canDelete = 0;
    prBool canCalcSizes = 0;
    prBool noFile = 0;
    prBool avoidAudioConform = 0;
    prBool canProvidePeakAudio = 0;
    csSDK_int32 priority = 0;
};

/// @brief Extended imFileAccessRec8 structure carrying UTF-16 filepath.
struct ImFileAccessRec8Ext {
    std::u16string filepath;
    csSDK_uint32 filetype = 0;
};

/// @brief Extended imOpenFileRec8 structure for memory reporting and private instance handle.
struct ImOpenFileRec8Ext {
    ImFileAccessRec8Ext fileinfo;
    void* privatedata = nullptr;
    csSDK_uint64 outExtraMemoryUsage = 0;
};

/// @brief Extended imImageInfoRec structure.
struct ImImageInfoRecExt {
    csSDK_int32 imageWidth = 0;
    csSDK_int32 imageHeight = 0;
    csSDK_uint32 pixelAspectNum = 1;
    csSDK_uint32 pixelAspectDen = 1;
    prBool supportsGetSourceVideo = 0;
    prBool supportsAsyncIO = 0;
};

/// @brief Extended imFileInfoRec8 structure populated during imGetInfo8.
struct ImFileInfoRec8Ext {
    void* privatedata = nullptr;
    csSDK_int32 streamIdx = 0;
    prBool hasVideo = 0;
    prBool hasAudio = 0;
    csSDK_uint32 vidDuration = 0;
    csSDK_int32 vidScale = 30;
    csSDK_int32 vidSampleSize = 1;
    ImImageInfoRecExt vidInfo;
    csSDK_uint64 audDuration = 0;
    double audSampleRate = 48000.0;
    csSDK_int32 audNumChannels = 0;
    csSDK_uint32 accessModes = 0;
    prBool highMemUsage = 0;
    prBool streamsAsComp = 0;
};

/// @brief Extended imIndFormatRec structure populated during imGetIndFormat.
struct ImIndFormatRecExt {
    csSDK_uint32 filetype = 0;
    char formatName[64]{};
    char formatShortName[32]{};
    char extension[16]{};
};

/// @brief Extended imIndPixelFormatRec structure populated during imGetIndPixelFormat.
struct ImIndPixelFormatRecExt {
    csSDK_int32 index = 0;
    csSDK_uint32 outPixelFormat = 0;
};

/// @brief Extended imSourceVideoRec structure populated during imGetSourceVideo.
struct ImSourceVideoRecExt {
    void* privatedata = nullptr;
    PrTime inFrame = 0;
    csSDK_uint32 inPixelFormat = kPixFmtBGRA4444_8u;
    PPixHand* outFrame = nullptr;
    const PrSDKPPixSuite* ppixSuite = nullptr;
};

/// @brief Extended imImportAudioRec7 structure for 32-bit float planar audio delivery.
struct ImImportAudioRec7Ext {
    void* privatedata = nullptr;
    PrTime position = 0;
    csSDK_uint32 size = 0;
    float** buffer = nullptr;
    csSDK_int32 numChannels = 0;
    double sampleRate = 48000.0;
};

/// @brief Extended imPeakAudioRec structure for in-memory min/max waveform generation.
struct ImPeakAudioRecExt {
    void* privatedata = nullptr;
    PrTime startSample = 0;
    csSDK_uint32 numSampleFrames = 0;
    csSDK_uint32 decimationFactor = 256;
    std::vector<std::vector<float>> minPeaks;
    std::vector<std::vector<float>> maxPeaks;
};

/// @brief Extended imAudioChannelLayoutRec structure for up to 32 channels.
struct ImAudioChannelLayoutRecExt {
    csSDK_int32 numChannels = 0;
    std::vector<PrAudioChannelLabelId> channelLabels;
};

/// @brief Async importer callback signature.
using AsyncImporterEntryFn = csSDK_int32 (*)(csSDK_int32 selector, imStdParms* stdParms, void* param1, void* param2);
using AsyncImporterCloseFn = csSDK_int32 (*)(void* asyncPrivateData);

/// @brief Extended imAsyncImporterCreationRec structure.
struct ImAsyncImporterCreationRecExt {
    void* inPrivateData = nullptr;
    void* outAsyncPrivateData = nullptr;
    AsyncImporterEntryFn outAsyncEntry = nullptr;
    AsyncImporterCloseFn outAsyncClose = nullptr;
};

/// @brief Static dispatch table entry for xImportEntry / AsyncImporterEntry (CC <= 5).
struct SelectorHandlerEntry {
    csSDK_int32 selector;
    csSDK_int32 (*handler)(imStdParms* stdParms, void* param1, void* param2);
};

} // namespace openmedia::importer
