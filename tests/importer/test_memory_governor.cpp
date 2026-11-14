#include "importer/memory_governor.hpp"

#include <cassert>
#include <iostream>

using namespace openmedia::importer;

namespace {
bool gEvicted = false;
void onQuietEvict(void*) noexcept {
    gEvicted = true;
}
} // namespace

int main() {
    const uint64_t osRam = MemoryGovernor::queryAvailablePhysicalRamBytes();
    assert(osRam > 0);

    const uint64_t sim3Gb = 3ULL * 1024ULL * 1024ULL * 1024ULL;
    assert(MemoryGovernor::computeBudgetBytes(sim3Gb) == sim3Gb / 2ULL);

    ImOpenFileRec8Ext openRec{};
    ImFileInfoRec8Ext infoRec{};
    assert(MemoryGovernor::applyMemoryFlags(sim3Gb, &openRec, &infoRec) == kImNoErr);
    assert(openRec.outExtraMemoryUsage > 0);
    assert(infoRec.highMemUsage == kPrTrueVal);

    gEvicted = false;
    assert(MemoryGovernor::handleQuietFile(&onQuietEvict, nullptr) == kImNoErr);
    assert(gEvicted);

    std::cout << "[PASS] TC-2.3.4 MemoryGovernor 50% RAM cap & imQuietFile passed.\n";
    return 0;
}
