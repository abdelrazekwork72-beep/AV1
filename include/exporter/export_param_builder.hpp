#pragma once
/// @file export_param_builder.hpp
/// @brief Exporter parameter tree builder with SetParamsVersion(1001), color metadata, and Out-of-Process JSON IPC.

#include "exporter/exporter_types.hpp"

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace openmedia::exporter {

/// @brief Supported color metadata presets for exported video streams.
enum class ExportColorMetadata {
    BT709 = 0,
    BT2100_PQ,
    BT2100_HLG,
    Sony_SLog3
};

/// @brief Serializable exporter preset parameters.
struct ExporterPresetParams {
    csSDK_int32 version = kOpenMediaExporterParamsVersion;
    int32_t width = 1920;
    int32_t height = 1080;
    int32_t crf = 28;
    std::string codec = "libsvtav1";
    ExportColorMetadata colorMetadata = ExportColorMetadata::BT709;
};

/// @brief Active GPU decoder/encoder status exchanged with the out-of-process popup UI.
struct PopupGpuStatus {
    std::string activeDecoder = "none";
    std::string activeEncoder = "libsvtav1";
    bool hardwareAvailable = false;
};

/// @brief Out-of-process IPC payload carrying Config.toml path, GPU status, and codec presets.
struct PopupIpcMessage {
    std::string configTomlPath;
    PopupGpuStatus gpuStatus;
    ExporterPresetParams preset;
};

/// @brief Builds default Adobe parameter trees and serializes .epr presets.
class ExportParamBuilder {
public:
    /// @brief Generate default parameters and invoke SetParamsVersion(1001).
    static csSDK_int32 generateDefaults(ExporterPresetParams& outParams,
                                        csSDK_int32* outSuiteVersion = nullptr) noexcept;

    /// @brief Convert color metadata enum to canonical string token.
    static const char* colorMetadataToString(ExportColorMetadata meta) noexcept;

    /// @brief Parse color metadata string token (defaults to BT709 if unrecognized).
    static ExportColorMetadata parseColorMetadata(const std::string& value) noexcept;

    /// @brief Serialize parameters to a deterministic key=value .epr preset string.
    static std::string serializePreset(const ExporterPresetParams& params);

    /// @brief Deserialize .epr preset string, rejecting stale versions if invalid.
    static bool deserializePreset(const std::string& text, ExporterPresetParams& outParams);
};

/// @brief Lightweight Out-of-Process IPC helper exchanging json:-prefixed messages over StdIn/StdOut.
class PopupIpcBridge {
public:
    static constexpr const char* kJsonPrefix = "json:";

    /// @brief Build CLI launch arguments (--popup --owner <HWND>) for the isolated popup process.
    static std::vector<std::string> buildLaunchArgs(uint64_t ownerHwnd);

    /// @brief Serialize a PopupIpcMessage to a single json:-prefixed line.
    static std::string serializeJsonLine(const PopupIpcMessage& msg);

    /// @brief Parse a json:-prefixed line into PopupIpcMessage.
    static bool parseJsonLine(const std::string& line, PopupIpcMessage& outMsg);

    /// @brief Write a request line to out and read the json:-prefixed response from in.
    static bool exchangeOverStreams(std::istream& in,
                                    std::ostream& out,
                                    const PopupIpcMessage& request,
                                    PopupIpcMessage& outResponse);
};

} // namespace openmedia::exporter
