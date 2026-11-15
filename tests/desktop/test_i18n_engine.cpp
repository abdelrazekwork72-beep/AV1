#include "desktop/i18n_engine.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace openmedia::desktop;

    I18nEngine engine(UiLanguage::English);
    assert(engine.language() == UiLanguage::English);
    assert(!engine.isRtl());
    assert(engine.languageCode() == "en");
    assert(engine.keyCount() >= 20);

    const std::string enTitle = engine.tr("app.title");
    assert(!enTitle.empty() && enTitle != "app.title");

    engine.setLanguage(UiLanguage::Arabic);
    assert(engine.language() == UiLanguage::Arabic);
    assert(engine.isRtl());
    assert(engine.languageCode() == "ar");

    const std::string arTitle = engine.tr("app.title");
    assert(!arTitle.empty() && arTitle != "app.title" && arTitle != enTitle);
    assert(engine.tr("nonexistent.key") == "nonexistent.key");

    std::cout << "test_i18n_engine: PASSED\n";
    return 0;
}
