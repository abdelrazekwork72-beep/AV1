#include "desktop/i18n_engine.hpp"

namespace openmedia::desktop {

I18nEngine::I18nEngine(UiLanguage initialLang) : lang_(initialLang) {
    en_ = {
        {"app.title", "OpenMedia Control Center v3.0 — Studio Edition"},
        {"app.popup_title", "OpenMedia Host Settings (Premiere / AE / AME)"},
        {"lang.toggle", "Language / اللغة: English (LTR)"},
        {"tab.settings", "Settings (Config.toml)"},
        {"tab.gpu_status", "GPU Status"},
        {"tab.cache_tools", "Cache & !options.influx"},
        {"tab.guide_health", "User Guide & Health"},
        {"settings.policy_label", "Importer Override Policy"},
        {"settings.policy_force", "Force OpenMedia to override all Adobe importers"},
        {"settings.policy_default", "Use OpenMedia for new formats, codecs & fallback (Default)"},
        {"settings.policy_new_only", "Use OpenMedia for new formats & fallback only"},
        {"settings.policy_disabled", "Disable OpenMedia Importer"},
        {"settings.gpu_mode_label", "GPU Hardware Decode Mode"},
        {"settings.gpu_recommended", "GPU for recommended media (>= 2560x1600, max 4 streams)"},
        {"settings.gpu_available", "GPU decode when available (All compatible codecs)"},
        {"settings.gpu_disabled", "GPU disabled (CPU software decode only)"},
        {"settings.extensions_label", "Override Extensions (comma-separated, e.g. mkv, webm, mov)"},
        {"settings.ram_percent", "Max Available RAM Usage (%)"},
        {"settings.frame_cache_mb", "Max Frame Cache Size (MB)"},
        {"settings.thumb_tc", "Thumbnail Timecode Overlay"},
        {"settings.duration_mode", "Duration Calculation Mode"},
        {"settings.reduce_warnings", "Reduce unnecessary VFR / seeking warnings"},
        {"settings.debug_logs", "Write verbose debug logs"},
        {"settings.disable_thumb_cache", "Disable persistent thumbnail cache"},
        {"settings.disable_frame_cache", "Disable frame cache"},
        {"settings.disable_seek_table", "Disable seeking table"},
        {"settings.disable_mt", "Disable multithreading decoding"},
        {"btn.save", "Save Settings"},
        {"btn.reset", "Reset to Default"},
        {"btn.close", "Close"},
        {"gpu.header", "Active Media Streams Across Adobe Applications"},
        {"gpu.empty", "No active video streams currently decoded."},
        {"gpu.badge_active", "GPU Active"},
        {"gpu.badge_cpu", "CPU Software"},
        {"cache.clear_thumbs", "Clear PrPro Thumbnails Cache"},
        {"cache.clear_premiere", "Clear Premiere Pro Media Cache"},
        {"cache.export_logs", "Export Logs & Telemetry (.zip)"},
        {"suffix.header", "Interactive !options.influx Batch Filename Builder"},
        {"suffix.apply", "Generate / Rename File"},
        {"suffix.restore", "Restore Original Filename"},
        {"health.header", "MediaCore 7.0 Plugin & Runtime Health"},
        {"status.saved", "Settings saved! Restart Adobe apps for changes to take effect."}
    };

    ar_ = {
        {"app.title", "مركز تحكم OpenMedia v3.0 — إصدار الاستوديو"},
        {"app.popup_title", "إعدادات OpenMedia داخل Adobe (Premiere / AE / AME)"},
        {"lang.toggle", "اللغة / Language: العربية (RTL)"},
        {"tab.settings", "الإعدادات (Config.toml)"},
        {"tab.gpu_status", "حالة كارت الشاشة (GPU Status)"},
        {"tab.cache_tools", "الكاش وأداة !options.influx"},
        {"tab.guide_health", "دليل الاستخدام وفحص الإضافة"},
        {"settings.policy_label", "سياسة استيراد الملفات وتجاوز مستوردات Adobe"},
        {"settings.policy_force", "إجبار OpenMedia على تجاوز جميع مستوردات Adobe"},
        {"settings.policy_default", "استخدام OpenMedia للصيغ والكودكس الحديثة وكبديل احتياطي (افتراضي)"},
        {"settings.policy_new_only", "استخدام OpenMedia للصيغ الحديثة وكبديل احتياطي فقط"},
        {"settings.policy_disabled", "تعطيل مستورد OpenMedia بالكامل"},
        {"settings.gpu_mode_label", "وضع فك التشفير عبر كارت الشاشة (GPU)"},
        {"settings.gpu_recommended", "GPU للوسائط الموصى بها (>= 2560x1600، حد أقصى 4 مسارات)"},
        {"settings.gpu_available", "تفعيل GPU متى توفر (لكل الكودكس المدعومة)"},
        {"settings.gpu_disabled", "تعطيل GPU (المعالج CPU فقط)"},
        {"settings.extensions_label", "تجاوز الامتدادات (مفصولة بفاصلة، مثل: mkv, webm, mov)"},
        {"settings.ram_percent", "الحد الأقصى لاستهلاك الرام المتاحة (%)"},
        {"settings.frame_cache_mb", "حجم ذاكرة تخزين الإطارات (MB)"},
        {"settings.thumb_tc", "عرض الـ Timecode على الصور المصغرة"},
        {"settings.duration_mode", "طريقة حساب مدة الفيديو"},
        {"settings.reduce_warnings", "تقليل تحذيرات VFR والبحث الزمني المتكررة"},
        {"settings.debug_logs", "تسجيل السجلات التفصيلية (Debug Logs)"},
        {"settings.disable_thumb_cache", "تعطيل كاش الصور المصغرة"},
        {"settings.disable_frame_cache", "تعطيل كاش الإطارات"},
        {"settings.disable_seek_table", "تعطيل جدول البحث الزمني"},
        {"settings.disable_mt", "تعطيل فك التشفير متعدد الخيوط"},
        {"btn.save", "حفظ الإعدادات"},
        {"btn.reset", "استعادة الإعدادات الافتراضية"},
        {"btn.close", "إغلاق"},
        {"gpu.header", "مسارات الفيديو النشطة داخل برامج Adobe"},
        {"gpu.empty", "لا توجد ملفات فيديو مفتوحة حالياً."},
        {"gpu.badge_active", "GPU نشط"},
        {"gpu.badge_cpu", "معالج CPU"},
        {"cache.clear_thumbs", "تنظيف كاش الصور المصغرة (Thumbnails)"},
        {"cache.clear_premiere", "تنظيف كاش Premiere Pro Media Cache"},
        {"cache.export_logs", "تصدير ملفات السجلات والتشخيص (.zip)"},
        {"suffix.header", "مولد لواحق !options.influx لتعديل خصائص الملفات"},
        {"suffix.apply", "توليد اللاحقة / إعادة التسمية"},
        {"suffix.restore", "استعادة اسم الملف الأصلي"},
        {"health.header", "فحص سلامة إضافات MediaCore 7.0 والمكتبات"},
        {"status.saved", "تم حفظ الإعدادات! أعد تشغيل برامج Adobe لتطبيق التغييرات."}
    };
}

void I18nEngine::setLanguage(UiLanguage lang) noexcept { lang_ = lang; }
UiLanguage I18nEngine::language() const noexcept { return lang_; }
bool I18nEngine::isRtl() const noexcept { return lang_ == UiLanguage::Arabic; }
std::string I18nEngine::languageCode() const { return isRtl() ? "ar" : "en"; }

const std::string& I18nEngine::tr(const std::string& key) const {
    const auto& map = (lang_ == UiLanguage::Arabic) ? ar_ : en_;
    const auto it = map.find(key);
    if (it != map.end()) {
        return it->second;
    }
    const auto enIt = en_.find(key);
    return (enIt != en_.end()) ? enIt->second : key;
}

size_t I18nEngine::keyCount() const noexcept { return en_.size(); }

} // namespace openmedia::desktop
