#include "exporter/chroma_fallback_guard.hpp"
#include <cassert>
#include <iostream>
using namespace openmedia::exporter;
int main() {
    assert(ChromaFallbackGuard::selectAv1Profile(444, 10) == 2 && ChromaFallbackGuard::selectAv1Profile(420, 12) == 2);
    bool notified = false;
    assert(ChromaFallbackGuard::resolveWithFallback("av1_nvenc", false, notified) == "libsvtav1" && notified);
    notified = false;
    assert(ChromaFallbackGuard::resolveWithFallback("av1_vaapi", true, notified, 444, 10) == "libsvtav1" && notified);
    std::cout << "[PASS] TC-3.3.6 ChromaFallbackGuard Profile 2 & 4:4:4/avcodec_open2 fallback passed.\n";
    return 0;
}
