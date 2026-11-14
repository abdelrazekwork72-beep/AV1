#include "exporter/nvenc_encoder.hpp"
namespace openmedia::exporter {
bool NvencEncoder::isDriverSupported(double driverVersion) noexcept {
    return driverVersion >= kMinDriverVersion;
}
std::vector<std::string> NvencEncoder::filterAv1Options(const std::vector<std::string>& opts) {
    std::vector<std::string> out;
    out.reserve(opts.size());
    for (const auto& o : opts) {
        if (o.find("qp_cr_offset") == std::string::npos) {
            out.push_back(o);
        }
    }
    return out;
}
} // namespace openmedia::exporter
