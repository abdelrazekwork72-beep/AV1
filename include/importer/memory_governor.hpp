#pragma once
/// @file memory_governor.hpp
/// @brief Dynamic 50% physical RAM governor and imQuietFile cache eviction handler.

#include "importer/importer_types.hpp"

#include <cstdint>

namespace openmedia::importer {

/// @brief Callback signature for cache eviction during imQuietFile.
using QuietEvictCallback = void (*)(void* userData) noexcept;

/// @brief Enforces a strict 50% available physical RAM budget and handles imQuietFile.
class MemoryGovernor {
public:
    /// @brief Query available physical RAM from the host OS in bytes.
    /// @return Available physical RAM in bytes (non-zero fallback if unavailable).
    static uint64_t queryAvailablePhysicalRamBytes() noexcept;

    /// @brief Compute the strict 50% RAM budget for a given available RAM figure.
    /// @param availableRamBytes Available physical RAM in bytes.
    /// @return Maximum allowed cache budget in bytes (at most 50%).
    static uint64_t computeBudgetBytes(uint64_t availableRamBytes) noexcept;

    /// @brief Populate imOpenFileRec8.outExtraMemoryUsage and imFileInfoRec8.highMemUsage.
    /// @param availableRamBytes Available physical RAM in bytes.
    /// @param openRec Optional open file record to update.
    /// @param infoRec Optional file info record to update.
    /// @return kImNoErr on success.
    static csSDK_int32 applyMemoryFlags(uint64_t availableRamBytes,
                                        ImOpenFileRec8Ext* openRec,
                                        ImFileInfoRec8Ext* infoRec) noexcept;

    /// @brief Handle selector imQuietFile by invoking the registered eviction callback.
    /// @param evictCb Eviction callback to release cached buffers.
    /// @param userData Opaque user pointer forwarded to evictCb.
    /// @return kImNoErr.
    static csSDK_int32 handleQuietFile(QuietEvictCallback evictCb, void* userData) noexcept;
};

} // namespace openmedia::importer
