#include "exporter/progress_watchdog.hpp"
#include <cassert>
#include <iostream>
using namespace openmedia::exporter;
int main() {
    ProgressWatchdog wd;
    assert(wd.update(0.25, true, false, false) == kExNoErr);
    assert(wd.resumeCount() == 1);
    assert(wd.update(0.50, false, true, false) == kExNoErr);
    assert(wd.update(0.52, false, true, true) == kExAbort);
    assert(wd.isAborted());
    std::cout << "[PASS] TC-3.2.4 ProgressWatchdog WaitForResume & GOP abort passed.\n";
    return 0;
}
