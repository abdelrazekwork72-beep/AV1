#pragma once
/// @file audio_conform_bypass.hpp
/// @brief Zero-conform protocol enforcement (imInit & imGetInfo8) with random-access seek.

#include "importer/importer_types.hpp"

#include <cstdint>

namespace openmedia::importer {

/// @brief Configures Adobe zero-conform flags and random-access sample positioning.
class AudioConformBypass {
public:
    /// @brief Configure imInit flags to bypass Premiere disk audio conforming.
    /// @param info Init info record.
    /// @return kImNoErr on success, kImMemErr if null.
    static csSDK_int32 configureInit(ImImportInfoRecExt* info) noexcept;

    /// @brief Configure imGetInfo8 accessModes for kRandomAccessImport.
    /// @param fileInfo File info record.
    /// @return kImNoErr on success, kImMemErr if null.
    static csSDK_int32 configureFileInfo(ImFileInfoRec8Ext* fileInfo) noexcept;

    /// @brief Compute byte/packet offset for an arbitrary sample index.
    static int64_t sampleToByteOffset(int64_t sampleIndex, uint32_t sampleRate, uint32_t avgBitrateBps) noexcept;
};

} // namespace openmedia::importer
