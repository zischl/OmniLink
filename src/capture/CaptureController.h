#pragma once
#include "Capture.h"
#include "EncodeStream.h"
#include "OmniEnums.h"
#include "nvenc.h"
#include <unordered_map>
#include <variant>

#if defined(_WIN32)

enum CaptureMode { DXGI, WGC, WGC_Window };

using EncodeStreamTypes = std::variant<
    EncodeStream<ScreenCaptureDXGI, BufferedNvencSession<StaticNvencSession>,    OmniNetSubStream>,
    EncodeStream<ScreenCaptureWGC,  BufferedNvencSession<CachedPoolNvencSession>, OmniNetSubStream>,
    EncodeStream<WindowCaptureWGC,  BufferedNvencSession<CachedPoolNvencSession>, OmniNetSubStream>>;
#elif defined(__linux__)

enum CaptureMode { PW, X11_SHM };

using EncodeStreamTypes = std::variant<EncodeStream<ScreenCapturePW, NvencSession, OmniNetSubStream>>;
#endif

struct OmniStreamController
{
    using StreamID = size_t;

  private:
    std::unordered_map<size_t, EncodeStreamTypes> Streams;
    size_t StreamCount = 0;

  public:
    NVENCODER NvEncodeAPI;

#if defined(_WIN32)
    StreamID AddStream(
        ID3D11Device*        D3D11Device,
        ID3D11DeviceContext* D3D11Context,
        OmniNetSubStream*    SubStream,
        DeviceMap            TargetID,
        CaptureMode          Mode,
        const StreamConfig&  Config = {}
    );
#elif defined(__linux__)
    StreamID AddStream(
        OmniNetSubStream* SubStream, DeviceMap TargetID, CaptureMode Mode, const StreamConfig& Config = {}
    );
#endif

    void RemoveStream(size_t StreamID);

    void StopAll();
};
