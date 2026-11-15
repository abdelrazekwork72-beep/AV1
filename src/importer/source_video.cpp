#include "importer/source_video.hpp"
#include "importer/frame_scaler.hpp"

#include <cstring>

namespace openmedia::importer {
namespace {

bool hasValidSuite(const PrSDKPPixSuite* suite) noexcept {
    return suite != nullptr && suite->GetPixels != nullptr && suite->GetRowBytes != nullptr;
}

uint32_t plane0BytesPerPixel(PixelFormat format) noexcept {
    struct FmtBppEntry {
        PixelFormat fmt;
        uint32_t bpp;
    };
    static constexpr FmtBppEntry kTable[] = {
        {PixelFormat::BGRA8, 4U},
        {PixelFormat::RGBA8, 4U},
        {PixelFormat::YUV420P10LE, 2U},
        {PixelFormat::YUV422P10LE, 2U},
        {PixelFormat::YUV444P10LE, 2U},
        {PixelFormat::YUV420P12LE, 2U},
        {PixelFormat::YUV422P12LE, 2U},
        {PixelFormat::YUV444P12LE, 2U},
        {PixelFormat::P010LE, 2U},
    };
    for (const auto& entry : kTable) {
        if (entry.fmt == format) {
            return entry.bpp;
        }
    }
    return 1U;
}

uint32_t computeMinRowBytes(const RawVideoFrame& frame) noexcept {
    return static_cast<uint32_t>(frame.width) * plane0BytesPerPixel(frame.format);
}

bool isValidFrameGeometry(const RawVideoFrame& frame) noexcept {
    const bool validWidth = frame.width >= SourceVideoHandler::kMinFrameDimension &&
                            frame.width <= SourceVideoHandler::kMaxFrameDimension;
    const bool validHeight = frame.height >= SourceVideoHandler::kMinFrameDimension &&
                             frame.height <= SourceVideoHandler::kMaxFrameDimension;
    return validWidth && validHeight;
}

bool hasValidPlane0(const RawVideoFrame& frame, uint32_t minRowBytes) noexcept {
    const auto requiredBytes = static_cast<std::size_t>(frame.height) * frame.rowBytes[0];
    return frame.rowBytes[0] >= minRowBytes && frame.planes[0].size() >= requiredBytes;
}

bool decodeValidatedFrame(ICoreVideoDecoder& decoder,
                          RawVideoFrame& outFrame,
                          uint32_t& outMinRowBytes) noexcept {
    if (decoder.decodeNextFrame(outFrame) != CoreStatus::Ok) {
        return false;
    }
    outMinRowBytes = computeMinRowBytes(outFrame);
    return isValidFrameGeometry(outFrame) && hasValidPlane0(outFrame, outMinRowBytes);
}

void disposeOnError(const PrSDKPPixSuite* suite, PPixHand ppix) noexcept {
    if (ppix != nullptr && suite != nullptr && suite->Dispose != nullptr) {
        suite->Dispose(ppix);
    }
}

bool lockAndQueryPPix(const PrSDKPPixSuite* suite,
                      PPixHand ppix,
                      char** outDstBuf,
                      csSDK_int32* outRowBytes,
                      uint32_t minRowBytes) noexcept {
    if (suite->GetPixels(ppix, kPPixBufferAccessWriteOnly, outDstBuf) != suiteError_NoError ||
        *outDstBuf == nullptr) {
        return false;
    }
    if (suite->GetRowBytes(ppix, outRowBytes) != suiteError_NoError || *outRowBytes <= 0) {
        return false;
    }
    return static_cast<uint32_t>(*outRowBytes) >= minRowBytes;
}

void copyFrameRows(const RawVideoFrame& frame,
                   char* dstBuf,
                   csSDK_int32 dstStride,
                   uint32_t copyBytes) noexcept {
    const uint8_t* srcBase = frame.planes[0].data();
    const auto srcStride = static_cast<std::size_t>(frame.rowBytes[0]);
    const auto dstRowStride = static_cast<std::size_t>(dstStride);
    for (int32_t row = 0; row < frame.height; ++row) {
        const auto srcOffset = static_cast<std::size_t>(row) * srcStride;
        const auto dstOffset = static_cast<std::size_t>(row) * dstRowStride;
        std::memcpy(dstBuf + dstOffset, srcBase + srcOffset, copyBytes);
    }
}

csSDK_int32 allocateAndPopulatePPix(ImSourceVideoRecExt& rec,
                                    SourceVideoHandler::PPixCreatorFn creator,
                                    const RawVideoFrame& frame,
                                    uint32_t minRowBytes) noexcept {
    const prRect bounds{0, 0, frame.width, frame.height};
    PPixHand ppix = nullptr;
    const auto fmt = static_cast<PrPixelFormat>(rec.inPixelFormat);
    if (creator(&ppix, kPPixBufferAccessWriteOnly, fmt, &bounds) != suiteError_NoError ||
        ppix == nullptr) {
        return kImMemErr;
    }

    char* dstBuf = nullptr;
    csSDK_int32 rowBytes = 0;
    if (!lockAndQueryPPix(rec.ppixSuite, ppix, &dstBuf, &rowBytes, minRowBytes)) {
        disposeOnError(rec.ppixSuite, ppix);
        return kImUnsupported;
    }

    if (frame.format != PixelFormat::BGRA8 && frame.format != PixelFormat::RGBA8) {
        FrameScaler scaler;
        if (scaler.scaleFrame(frame, reinterpret_cast<uint8_t*>(dstBuf), rowBytes,
                              PixelFormat::BGRA8, frame.width, frame.height) == CoreStatus::Ok) {
            *rec.outFrame = ppix;
            return kImNoErr;
        }
    }
    copyFrameRows(frame, dstBuf, rowBytes, minRowBytes);
    *rec.outFrame = ppix;
    return kImNoErr;
}

} // namespace

csSDK_int32 SourceVideoHandler::renderSourceVideo(ImSourceVideoRecExt& rec,
                                                  ICoreVideoDecoder& decoder,
                                                  PPixCreatorFn creator) noexcept {
    if (rec.outFrame == nullptr || creator == nullptr || !hasValidSuite(rec.ppixSuite)) {
        return kImUnsupported;
    }
    *rec.outFrame = nullptr;

    RawVideoFrame frame;
    uint32_t minRowBytes = 0;
    if (!decodeValidatedFrame(decoder, frame, minRowBytes)) {
        return kImUnsupported;
    }

    return allocateAndPopulatePPix(rec, creator, frame, minRowBytes);
}

csSDK_int32 SourceVideoHandler::renderSourceVideo(ImSourceVideoRecExt& rec,
                                                  ICoreVideoDecoder& decoder,
                                                  const PrSDKPPixCreatorSuite* creatorSuite) noexcept {
    if (creatorSuite == nullptr) {
        return kImUnsupported;
    }
    return renderSourceVideo(rec, decoder, creatorSuite->CreatePPix);
}

csSDK_int32 SourceVideoHandler::handleGetSourceVideo(imStdParms* stdParms,
                                                     ImSourceVideoRecExt* rec,
                                                     ICoreVideoDecoder* decoder,
                                                     PPixCreatorFn creator) noexcept {
    (void)stdParms;
    if (rec == nullptr || decoder == nullptr) {
        return kImUnsupported;
    }
    return renderSourceVideo(*rec, *decoder, creator);
}

} // namespace openmedia::importer
