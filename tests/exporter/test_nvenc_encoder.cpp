#include "exporter/nvenc_encoder.hpp"
#include <cassert>
#include <iostream>
using namespace openmedia::exporter;
int main() {
    assert(!NvencEncoder::isDriverSupported(565.90));
    assert(NvencEncoder::isDriverSupported(572.16));
    assert(NvencEncoder::validateSecureLibraryLoad("nvEncodeAPI64.dll", NvencEncoder::kLoadLibrarySearchSystem32));
    assert(!NvencEncoder::validateSecureLibraryLoad("nvEncodeAPI64.dll", 0U));
    assert(!NvencEncoder::validateSecureLibraryLoad("..\\evil\\nvEncodeAPI64.dll", NvencEncoder::kLoadLibrarySearchSystem32));
    auto filtered = NvencEncoder::filterAv1Options({"preset=p5", "qp_cr_offset=2", "cq=24"});
    assert(filtered.size() == 2);
    std::cout << "[PASS] TC-3.3.2 NvencEncoder >=570.0, LOAD_LIBRARY_SEARCH_SYSTEM32 & qp_cr_offset filter passed.\n";
    return 0;
}
