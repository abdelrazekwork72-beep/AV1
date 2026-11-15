#include "exporter/nvenc_encoder.hpp"
#include <cassert>
#include <iostream>
#include <vector>

using namespace openmedia::exporter;

namespace {
int g_nvencWarnCount = 0;
prSuiteError mockNvencWarn(csSDK_uint32, const char*, const char*) {
    ++g_nvencWarnCount;
    return 0;
}
} // namespace

int main() {
    assert(!NvencEncoder::isDriverSupported(565.90));
    assert(NvencEncoder::isDriverSupported(572.16));
    assert(NvencEncoder::validateSecureLibraryLoad("nvEncodeAPI64.dll", NvencEncoder::kLoadLibrarySearchSystem32));
    assert(!NvencEncoder::validateSecureLibraryLoad("nvEncodeAPI64.dll", 0U));
    assert(!NvencEncoder::validateSecureLibraryLoad("..\\evil\\nvEncodeAPI64.dll", NvencEncoder::kLoadLibrarySearchSystem32));
    auto filtered = NvencEncoder::filterAv1Options({"preset=p5", "qp_cr_offset=2", "cq=24"});
    assert(filtered.size() == 2);

    assert(NvencEncoder::shouldFallbackMidRender(1, false));
    assert(NvencEncoder::shouldFallbackMidRender(0, true));
    assert(!NvencEncoder::shouldFallbackMidRender(0, false));

    g_nvencWarnCount = 0;
    PrSDKErrorSuite suite{};
    suite.SetEventString = &mockNvencWarn;
    FallbackSessionState state{};
    std::vector<openmedia::RawVideoFrame> pending(2);
    std::vector<openmedia::RawVideoFrame> flushed;
    assert(NvencEncoder::handleMidRenderFailure(pending, flushed, state, &suite, true) == "libsvtav1");
    assert(pending.empty() && flushed.size() == 2 && g_nvencWarnCount == 1);

    std::cout << "[PASS] TC-3.3.2 NvencEncoder >=570.0, DLL safety, and mid-render drain fallback passed.\n";
    return 0;
}
