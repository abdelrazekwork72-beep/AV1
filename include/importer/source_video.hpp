#pragma once
/// @file source_video.hpp
/// @brief MediaCore `imGetSourceVideo` handler and `PPixHand` allocation bridge for US-2.2.0 Unit 1.

#include "PrSDKPixelFormat.h"
#ifndef PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_709
#define PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_709 \
    PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u
#endif

#include "PrSDKPPixCreatorSuite.h"
#include "importer/importer_types.hpp"

#include <cstdint>

namespace openmedia::importer {

/// @brief Handles MediaCore `imGetSourceVideo` requests by decoding frames and populating `PPixHand` buffers.
class SourceVideoHandler final {
public:
    /// @brief Callback signature matching `PrSDKPPixCreatorSuite::CreatePPix`.
    /// @param outPPix Output handle receiving the newly allocated `PPixHand`.
    /// @param access Requested buffer access mode (e.g., `kPPixBufferAccessWriteOnly`).
    /// @param format Negotiated Premiere Pro pixel format.
    /// @param bounds Frame bounds rectangle (`0, 0, width, height`).
    /// @return `suiteError_NoError` on success, or an error code such as `suiteError_OutOfMemory`.
    using PPixCreatorFn = prSuiteError (*)(PPixHand* outPPix,
                                          csSDK_int32 access,
                                          PrPixelFormat format,
                                          const prRect* bounds);

    /// @brief Minimum supported frame width and height in pixels (security bound TM-2.2.2).
    static constexpr int32_t kMinFrameDimension = 16;

    /// @brief Maximum supported frame width and height in pixels (security bound TM-2.2.2).
    static constexpr int32_t kMaxFrameDimension = 16384;

    /// @brief Decodes the next video frame and copies its pixel rows into a newly allocated `PPixHand`.
    /// @param rec Extended source video record containing target pixel format, `ppixSuite`, and `outFrame`.
    /// @param decoder Video decoder instance providing decoded `RawVideoFrame` data.
    /// @param creator Callback used to allocate the destination `PPixHand` buffer.
    /// @return `kImNoErr` on success, `kImMemErr` on allocation failure, or `kImUnsupported` on invalid inputs or decode failure.
    static csSDK_int32 renderSourceVideo(ImSourceVideoRecExt& rec,
                                         ICoreVideoDecoder& decoder,
                                         PPixCreatorFn creator) noexcept;

    /// @brief Overload accepting a `PrSDKPPixCreatorSuite` pointer and delegating to `renderSourceVideo`.
    /// @param rec Extended source video record containing target pixel format, `ppixSuite`, and `outFrame`.
    /// @param decoder Video decoder instance providing decoded `RawVideoFrame` data.
    /// @param creatorSuite Pointer to the Premiere Pro PPix creator suite.
    /// @return `kImNoErr` on success, `kImMemErr` on allocation failure, or `kImUnsupported` on invalid inputs or decode failure.
    static csSDK_int32 renderSourceVideo(ImSourceVideoRecExt& rec,
                                         ICoreVideoDecoder& decoder,
                                         const PrSDKPPixCreatorSuite* creatorSuite) noexcept;

    /// @brief Entry-point helper for dispatching the `imGetSourceVideo` MediaCore selector.
    /// @param stdParms Standard MediaCore importer parameters.
    /// @param rec Pointer to `ImSourceVideoRecExt` associated with the request.
    /// @param decoder Pointer to the active `ICoreVideoDecoder`.
    /// @param creator Callback used to allocate the destination `PPixHand`.
    /// @return `kImNoErr` on success, `kImMemErr` on allocation failure, or `kImUnsupported` on error.
    static csSDK_int32 handleGetSourceVideo(imStdParms* stdParms,
                                            ImSourceVideoRecExt* rec,
                                            ICoreVideoDecoder* decoder,
                                            PPixCreatorFn creator) noexcept;
};

} // namespace openmedia::importer
