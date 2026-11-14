#pragma once
/// @file file_probe.hpp
/// @brief File probing, UTF-16 path conversion, native Adobe delegation, and rational timebase mapping.

#include "PrSDKPixelFormat.h"
#ifndef PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_709
#define PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_709 PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u
#endif
#include "importer/importer_types.hpp"

#include <string>
#include <vector>

namespace openmedia::importer {

/// @brief Handles imOpenFile8 probing, native format delegation, and imGetInfo8 population.
class FileProbe {
public:
    /// @brief Converts a UTF-16 path string (BMP and surrogate pairs) into UTF-8.
    /// @param utf16 Input UTF-16 string.
    /// @return UTF-8 encoded string; unpaired surrogates are replaced with U+FFFD.
    static std::string utf16ToUtf8(const std::u16string& utf16);

    /// @brief Determines whether a container should be delegated back to Adobe's native importer.
    /// @param streams Discovered streams from the demuxer.
    /// @param ext File extension (with or without leading dot, case-insensitive).
    /// @return True for .mp4/.mov containers that contain no modern plugin codecs (AV1/VP9/VP8/Opus/FLAC/Vorbis).
    static bool shouldDelegateToNative(const std::vector<StreamInfo>& streams, const std::string& ext);

    /// @brief Opens a media file via ICoreDemuxer and checks for native delegation.
    /// @param openRec Extended imOpenFileRec8 structure containing the UTF-16 filepath.
    /// @param demuxer Demuxer instance used to open and probe container streams.
    /// @return kImNoErr on success, or kImBadFile on open failure or native delegation.
    static csSDK_int32 openAndProbe(ImOpenFileRec8Ext& openRec, ICoreDemuxer& demuxer);

    /// @brief Populates ImFileInfoRec8Ext capabilities, rational video timebase, and audio metadata.
    /// @param streams Discovered container streams.
    /// @param infoRec Extended imFileInfoRec8 structure to populate.
    /// @return kImNoErr on success, or kImBadFile if no streams are present.
    static csSDK_int32 populateFileInfo(const std::vector<StreamInfo>& streams, ImFileInfoRec8Ext& infoRec);
};

} // namespace openmedia::importer
