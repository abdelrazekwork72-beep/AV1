#pragma once
/// @file i18n_engine.hpp
/// @brief Bilingual English (LTR) / Arabic (RTL) localization engine for OpenMediaDesktop.

#include "desktop/desktop_types.hpp"

#include <string>
#include <unordered_map>

namespace openmedia::desktop {

/// @brief Provides instant English (LTR) and Arabic (RTL) string resolution.
class I18nEngine {
public:
    explicit I18nEngine(UiLanguage initialLang = UiLanguage::English);

    void setLanguage(UiLanguage lang) noexcept;
    UiLanguage language() const noexcept;
    bool isRtl() const noexcept;
    std::string languageCode() const;

    /// @brief Translates a canonical UI key; returns key if not found.
    const std::string& tr(const std::string& key) const;

    /// @brief Returns total number of registered translation keys.
    size_t keyCount() const noexcept;

private:
    UiLanguage lang_;
    std::unordered_map<std::string, std::string> en_;
    std::unordered_map<std::string, std::string> ar_;
};

} // namespace openmedia::desktop
