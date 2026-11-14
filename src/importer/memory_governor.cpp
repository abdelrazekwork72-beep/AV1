#include "importer/memory_governor.hpp"

#include <fstream>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#endif

namespace openmedia::importer {

uint64_t MemoryGovernor::queryAvailablePhysicalRamBytes() noexcept {
#if defined(_WIN32)
    MEMORYSTATUSEX statex{};
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
        return static_cast<uint64_t>(statex.ullAvailPhys);
    }
#elif defined(__APPLE__)
    uint64_t memSize = 0;
    size_t len = sizeof(memSize);
    if (sysctlbyname("hw.memsize", &memSize, &len, nullptr, 0) == 0 && memSize > 0) {
        return memSize / 2ULL;
    }
#elif defined(__linux__)
    std::ifstream meminfo("/proc/meminfo");
    std::string key;
    uint64_t kb = 0;
    std::string unit;
    while (meminfo >> key >> kb >> unit) {
        if (key == "MemAvailable:") {
            return kb * 1024ULL;
        }
    }
#endif
    return 1024ULL * 1024ULL * 1024ULL;
}

uint64_t MemoryGovernor::computeBudgetBytes(uint64_t availableRamBytes) noexcept {
    return availableRamBytes / 2ULL;
}

csSDK_int32 MemoryGovernor::applyMemoryFlags(uint64_t availableRamBytes,
                                             ImOpenFileRec8Ext* openRec,
                                             ImFileInfoRec8Ext* infoRec) noexcept {
    const uint64_t budget = computeBudgetBytes(availableRamBytes);
    if (openRec != nullptr) {
        constexpr uint64_t kMaxInt32 = 0x7FFFFFFFULL;
        openRec->outExtraMemoryUsage = static_cast<csSDK_int32>(
            budget > kMaxInt32 ? kMaxInt32 : budget);
    }
    if (infoRec != nullptr) {
        infoRec->highMemUsage = kPrTrueVal;
    }
    return kImNoErr;
}

csSDK_int32 MemoryGovernor::handleQuietFile(QuietEvictCallback evictCb, void* userData) noexcept {
    if (evictCb != nullptr) {
        evictCb(userData);
    }
    return kImNoErr;
}

} // namespace openmedia::importer
