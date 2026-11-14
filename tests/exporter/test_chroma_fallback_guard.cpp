#include "exporter/chroma_fallback_guard.hpp"
#include <cassert>
#include <iostream>
using namespace openmedia::exporter;
int main() {
    assert(ChromaFallbackGuard::selectAv1Profile(444) == 2);
    bool notified = false;
    assert(ChromaFallbackGuard::resolveWithFallback("av1_nvenc", false, notified) == "libsvtav1");
    assert(notified);
    std::cout << "[PASS] TC-3.3.6 ChromaFallbackGuard Profile 2 & libsvtav1 fallback passed.\n";
    return 0;
}
