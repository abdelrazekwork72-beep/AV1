#include "desktop/desktop_main_window.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace openmedia::desktop;

    const char* argv[] = {"OpenMediaDesktop", "--popup", "--lang", "ar", "--headless-self-test"};
    auto opts = DesktopMainWindow::parseCommandLine(5, const_cast<char**>(argv));
    assert(opts.popupMode == true);
    assert(opts.headlessSelfTest == true);
    assert(opts.langOverrideSet == true);
    assert(opts.langOverride == UiLanguage::Arabic);

    DesktopMainWindow win(opts);
    win.initialize();
    assert(win.isPopupMode() == true);
    assert(win.language() == UiLanguage::Arabic);
    assert(win.isRtlLayout() == true);

    win.setLanguage(UiLanguage::English);
    assert(!win.isRtlLayout());

    win.selectTab(DesktopTab::SuffixModifiers);
    assert(win.activeTab() == DesktopTab::SuffixModifiers);

    assert(win.run() == 0);

    std::cout << "test_desktop_main_window: PASSED\n";
    return 0;
}
