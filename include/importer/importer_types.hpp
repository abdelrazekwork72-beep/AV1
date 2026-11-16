#pragma once
/// @file importer_types.hpp
/// @brief Shared Adobe MediaCore Importer bridge types, official selectors, return codes,
///        and binary-aligned host structs for Module 2 (Premiere Pro / Media Encoder x64 ABI).

#include "openmedia/core_types.hpp"

#include <cstddef>
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
#define PREMPLUGENTRY csSDK_int32
#endif

namespace openmedia::importer {

/// @brief Unique FourCC for OpenMedia Importer ('OMIP'), strictly avoiding After Effects reserved 'MOov' (0x4D4F6F76).
inline constexpr uint32_t kOpenMediaImporterFourCC = 0x4F4D4950U;
static_assert(kOpenMediaImporterFourCC != 0x4D4F6F76U,
              "FourCC must not collide with After Effects reserved 0x4D4F6F76");

/// @brief Official Adobe Premiere Pro MediaCore Importer Selectors (0..82) + Legacy Mock Aliases.
inline constexpr csSDK_int32 kSelInit = 0;                       // imInit
inline constexpr csSDK_int32 kSelShutdown = 1;                   // imShutdown
inline constexpr csSDK_int32 kSelImportImage = 5;                // imImportImage
inline constexpr csSDK_int32 kSelQuietFile = 8;                  // imQuietFile (official)
inline constexpr csSDK_int32 kSelCloseFile = 9;                  // imCloseFile (official)
inline constexpr csSDK_int32 kSelAnalysis = 12;                  // imAnalysis
inline constexpr csSDK_int32 kSelDataRateAnalysis = 13;          // imDataRateAnalysis
inline constexpr csSDK_int32 kSelGetIndFormat = 14;              // imGetIndFormat (official)
inline constexpr csSDK_int32 kSelGetMetaData = 23;               // imGetMetaData
inline constexpr csSDK_int32 kSelSetMetaData = 24;               // imSetMetaData
inline constexpr csSDK_int32 kSelImportAudio7 = 25;              // imImportAudio7 (official)
inline constexpr csSDK_int32 kSelGetIndPixelFormat = 27;         // imGetIndPixelFormat (official)
inline constexpr csSDK_int32 kSelGetSupports7 = 28;              // imGetSupports7 (official)
inline constexpr csSDK_int32 kSelGetSupports8 = 36;              // imGetSupports8 (official)
inline constexpr csSDK_int32 kSelGetInfo8 = 37;                  // imGetInfo8 (official)
inline constexpr csSDK_int32 kSelGetPrefs8 = 38;                 // imGetPrefs8 (official)
inline constexpr csSDK_int32 kSelOpenFile8 = 39;                 // imOpenFile8 (official)
inline constexpr csSDK_int32 kSelGetPreferredFrameSize = 45;     // imGetPreferredFrameSize (official)
inline constexpr csSDK_int32 kSelCreateAsyncImporter = 46;       // imCreateAsyncImporter (official)
inline constexpr csSDK_int32 kSelGetSourceVideo = 47;            // imGetSourceVideo (official)
inline constexpr csSDK_int32 kSelGetPeakAudio = 49;              // imGetPeakAudio (official)
inline constexpr csSDK_int32 kSelGetTimeInfo8 = 52;              // imGetTimeInfo8 (official)
inline constexpr csSDK_int32 kSelSetTimeInfo8 = 53;              // imSetTimeInfo8 (official)
inline constexpr csSDK_int32 kSelGetAudioChannelLayout = 64;     // imGetAudioChannelLayout (official)
inline constexpr csSDK_int32 kSelSelectClipFrameDescriptor = 65; // imSelectClipFrameDescriptor (official)
inline constexpr csSDK_int32 kSelGetIndColorSpace = 73;          // imGetIndColorSpace (official)
inline constexpr csSDK_int32 kSelGetInfo9 = 81;                  // imGetInfo9 (official)

/// @brief Official Adobe Premiere Pro MediaCore Importer Return Codes (prMalError).
inline constexpr csSDK_int32 kImNoErr = 0;                       // imNoErr / malNoError
inline constexpr csSDK_int32 kImTooWide = 1;                     // imTooWide
inline constexpr csSDK_int32 kImBadFile = 2;                     // imBadFile (official)
inline constexpr csSDK_int32 kImUnsupported = 3;                 // imUnsupported (official)
inline constexpr csSDK_int32 kImMemErr = 4;                      // imMemErr (official)
inline constexpr csSDK_int32 kImOtherErr = 5;                    // imOtherErr
inline constexpr csSDK_int32 kImNoContent = 6;                   // imNoContent
inline constexpr csSDK_int32 kImBadCodec = 9;                    // imBadCodec
inline constexpr csSDK_int32 kImNoTimecode = 12;                 // imNoTimecode
inline constexpr csSDK_int32 kImIterateStreams = 18;             // imIterateStreams (official)
inline constexpr csSDK_int32 kImBadStreamIndex = 19;             // imBadStreamIndex (official)
inline constexpr csSDK_int32 kImIterateFrameSizes = 24;          // imIterateFrameSizes
inline constexpr csSDK_int32 kImFrameNotFound = 28;              // imFrameNotFound
inline constexpr csSDK_int32 kImBadHeader = 29;                  // imBadHeader
inline constexpr csSDK_int32 kImFileOpenFailed = 31;             // imFileOpenFailed
inline constexpr csSDK_int32 kImFileHasNoImportableStreams = 32; // imFileHasNoImportableStreams
inline constexpr csSDK_int32 kImUnsupportedAudioFormat = 34;     // imUnsupportedAudioFormat
inline constexpr csSDK_int32 kImDecompressionError = 36;         // imDecompressionError
inline constexpr csSDK_int32 kImInvalidPreferences = 37;         // imInvalidPreferences
inline constexpr csSDK_int32 kMalUnknownError = 99;              // malUnknownError
inline constexpr csSDK_int32 kMalSupports7 = 100;                // malSupports7
inline constexpr csSDK_int32 kMalSupports8 = 101;                // malSupports8 (0x65)
inline constexpr csSDK_int32 kMalSupportsPerInstancePrefs = 102; // malSupportsPerInstancePrefs
inline constexpr csSDK_int32 kImDelegate = 255;                  // imDelegate
inline constexpr csSDK_int32 kImCancel = 300;                    // imCancel
inline constexpr csSDK_int32 kImBadFormatIndex = 301;            // imBadFormatIndex (0x12d)
inline constexpr csSDK_int32 kImIsCacheable = 400;               // imIsCacheable (0x190)

/// @brief Adobe boolean and access-mode constants.
inline constexpr prBool kPrTrueVal = 1;
inline constexpr prBool kPrFalseVal = 0;
inline constexpr csSDK_uint32 kRandomAccessImportFlag = 0x00000001U;
inline constexpr csSDK_int32 kPPixBufferAccessWriteOnly = 1;
inline constexpr csSDK_int32 kPPixBufferAccessReadOnly = 0;

/// @brief Extended pixel format constants for 10/12-bit HDR and alpha negotiation.
inline constexpr csSDK_uint32 kPixFmtBGRA4444_8u = PrPixelFormat_BGRA_4444_8u;
inline constexpr csSDK_uint32 kPixFmtVUYA4444_8u = PrPixelFormat_VUYA_4444_8u;
inline constexpr csSDK_uint32 kPixFmtYUV420Planar_8u_709 = PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_709;
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

// ============================================================================
// Binary-Exact Real Adobe Premiere Pro x64 Host ABI Layouts (#pragma pack(4))
// ============================================================================
#pragma pack(push, 4)

struct PrHostPlugSuites {
    std::uint8_t reserved0[0x38]{};
    SPBasicSuite* (*getSPBasicSuite)() = nullptr; // +0x38
};

struct PrHostPiSuites {
    std::uint32_t reserved0 = 0;
    void* utilFuncs = nullptr;            // +0x04
    std::uint8_t reserved1[0x10]{};
    PrHostPlugSuites* plugSuites = nullptr; // +0x1C
};

struct PrRawStdParms {
    csSDK_int32 imInterfaceVer = 0;       // +0x00
    std::uint8_t reserved0[8]{};          // +0x04
    PrHostPiSuites* piSuites = nullptr;   // +0x0C
};

struct PrRawImportInfoRec {
    std::uint8_t reserved0[0x08]{};
    csSDK_int64 privatedata = 0;          // +0x08
    std::uint8_t reserved1[0x14]{};
    csSDK_int32 canOpen = 0;              // +0x24
    csSDK_int32 canGetPrefs8 = 0;         // +0x28
    csSDK_int64 setupOnDblClk = 0;        // +0x2C
    csSDK_int32 hasSetup = 0;             // +0x34
    std::uint8_t reserved2[0x08]{};
    csSDK_int64 noFile = 0;               // +0x40
    csSDK_int32 avoidAudioConform = 0;    // +0x48
};
static_assert(offsetof(PrRawImportInfoRec, canOpen) == 0x24, "canOpen offset must be 0x24");
static_assert(offsetof(PrRawImportInfoRec, canGetPrefs8) == 0x28, "canGetPrefs8 offset must be 0x28");
static_assert(offsetof(PrRawImportInfoRec, hasSetup) == 0x34, "hasSetup offset must be 0x34");

struct PrRawIndFormatRec {
    csSDK_uint32 filetype = 0;            // +0x00
    csSDK_int32 flags = 0;                // +0x04 (0x10 = xfCanOpen | xfIsMovie)
    csSDK_int32 canAutoOpen = 0;          // +0x08
    char formatName[256]{};               // +0x0C
    char formatShortName[32]{};           // +0x10C
    char platformExtension[464]{};        // +0x12C (total size = 0x2FC = 764 bytes)
};
static_assert(sizeof(PrRawIndFormatRec) == 0x2FC, "PrRawIndFormatRec size must be 0x2FC");
static_assert(offsetof(PrRawIndFormatRec, formatName) == 0x0C, "formatName offset must be 0x0C");
static_assert(offsetof(PrRawIndFormatRec, platformExtension) == 0x12C, "platformExtension offset must be 0x12C");

struct PrRawGetPrefsRec {
    void* prefs = nullptr;                // +0x00
    csSDK_int32 prefsLength = 0;          // +0x08
    char firstTime = 0;                   // +0x0C
    char pad[3]{};
};
static_assert(offsetof(PrRawGetPrefsRec, prefsLength) == 0x08, "prefsLength offset must be 0x08");
static_assert(offsetof(PrRawGetPrefsRec, firstTime) == 0x0C, "firstTime offset must be 0x0C");

struct PrRawOpenFileRefRec8 {
    std::uint8_t reserved0[0x0C]{};
    const char16_t* filepathPtr = nullptr; // +0x0C
    std::uint8_t reserved1[0x10]{};
    void* privatedata = nullptr;          // +0x24
    std::uint8_t reserved2[0x14]{};
    csSDK_int32 inReadWrite = 0;          // +0x40
};
static_assert(offsetof(PrRawOpenFileRefRec8, privatedata) == 0x24, "privatedata offset must be 0x24");

struct PrRawFrameFormat {
    csSDK_int32 inWidth = 0;              // +0x00
    csSDK_int32 inHeight = 0;             // +0x04
    csSDK_uint32 inPixelFormat = 0;       // +0x08
};

struct PrRawSourceVideoRec {
    std::uint8_t reserved0[0x0C]{};
    PrTime inFrameTime = 0;               // +0x0C
    PrRawFrameFormat* inFrameFormats = nullptr; // +0x14
    std::uint8_t reserved1[0x08]{};
    PPixHand* outFrame = nullptr;         // +0x24
    void* prefs = nullptr;                // +0x2C
    void* privatedata = nullptr;          // +0x34
    std::uint8_t reserved2[0x10]{};
    csSDK_int32 inQuality = 0;            // +0x4C
};
static_assert(offsetof(PrRawSourceVideoRec, inFrameTime) == 0x0C, "inFrameTime offset must be 0x0C");
static_assert(offsetof(PrRawSourceVideoRec, inFrameFormats) == 0x14, "inFrameFormats offset must be 0x14");
static_assert(offsetof(PrRawSourceVideoRec, outFrame) == 0x24, "outFrame offset must be 0x24");

struct PrRawPreferredFrameSizeRec {
    std::uint8_t reserved0[0x08]{};
    csSDK_int32 inWidth = 0;              // +0x08
    csSDK_int32 inHeight = 0;             // +0x0C
    csSDK_uint32 inPixelFormat = 0;       // +0x10
    csSDK_int32 inIndex = 0;              // +0x14
    std::uint8_t reserved1[0x0C]{};
    csSDK_int32 outWidth = 0;             // +0x24
    csSDK_int32 outHeight = 0;            // +0x28
    std::uint8_t reserved2[0x0C]{};
    csSDK_int32 outSupportsScaling = 0;   // +0x38
};
static_assert(offsetof(PrRawPreferredFrameSizeRec, outWidth) == 0x24, "outWidth offset must be 0x24");

#pragma pack(pop)

// ============================================================================
// High-Level C++ Bridge Structures Used Across Module 2 Units & Tests
// ============================================================================

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
    prBool canGetPrefs8 = 1;
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
