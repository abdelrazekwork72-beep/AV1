#include "exporter/chroma_fallback_guard.hpp"
#include <cassert>
#include <iostream>
#include <vector>

using namespace openmedia::exporter;

namespace {
int g_warningCallCount = 0;
prSuiteError mockSetEventString(csSDK_uint32, const char*, const char*) {
    ++g_warningCallCount;
    return 0;
}
} // namespace

int main() {
    assert(ChromaFallbackGuard::selectAv1Profile(444, 10) == 2 && ChromaFallbackGuard::selectAv1Profile(420, 12) == 2);
    bool notified = false;
    assert(ChromaFallbackGuard::resolveWithFallback("av1_nvenc", false, notified) == "libsvtav1" && notified);
    notified = false;
    assert(ChromaFallbackGuard::resolveWithFallback("av1_vaapi", true, notified, 444, 10) == "libsvtav1" && notified);

    g_warningCallCount = 0;
    PrSDKErrorSuite errorSuite{};
    errorSuite.SetEventString = &mockSetEventString;
    FallbackSessionState state{};

    std::vector<openmedia::RawVideoFrame> pending1(3);
    std::vector<openmedia::RawVideoFrame> flushed1;
    assert(ChromaFallbackGuard::drainBeforeFallback("av1_amf", pending1, flushed1, state, &errorSuite, true) == "libsvtav1");
    assert(pending1.empty() && flushed1.size() == 3);
    assert(g_warningCallCount == 1 && state.warningEmitted && state.warningsSuppressed == 0);

    std::vector<openmedia::RawVideoFrame> pending2(2);
    std::vector<openmedia::RawVideoFrame> flushed2;
    assert(ChromaFallbackGuard::drainBeforeFallback("vp9_qsv", pending2, flushed2, state, &errorSuite, true) == "libvpx-vp9");
    assert(pending2.empty() && flushed2.size() == 2);
    assert(g_warningCallCount == 1 && state.warningsSuppressed == 1);

    std::cout << "[PASS] TC-3.3.6 ChromaFallbackGuard drainBeforeFallback & once-per-session warning dedup passed.\n";
    return 0;
}
