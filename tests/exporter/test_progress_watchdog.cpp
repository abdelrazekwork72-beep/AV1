#include "exporter/progress_watchdog.hpp"
#include <cassert>
#include <iostream>
using namespace openmedia::exporter;
int main() {
    ProgressWatchdog wd;
    assert(wd.update(0.30, true, false, false) == kExNoErr);
    assert(wd.resumeCount() == 1);
    assert(wd.update(0.60, false, true, false) == kExNoErr);
    assert(wd.update(0.61, false, true, false) == kExNoErr);
    assert(wd.update(0.62, false, true, false) == kExNoErr);
    assert(wd.update(0.63, false, true, false) == kExAbort);
    assert(wd.closingFramesUsed() <= ProgressWatchdog::kMaxGopClosingFrames && wd.isAborted());
    std::cout << "[PASS] TC-3.2.4 ProgressWatchdog WaitForResume & N<=4 GOP abort cap passed.\n";
    return 0;
}
