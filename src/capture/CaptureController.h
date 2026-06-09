#pragma once
#include "Capture.h"
#include "EncodeStream.h"
#include "OmniEnums.h"
#include "nvenc.h"
#include <unordered_map>
#include <variant>

#if defined(_WIN32)

enum CaptureMode { DXGI, WGC };

using EncodeStreamTypes =
    std::variant<EncodeStream<ScreenCaptureDXGI>, EncodeStream<ScreenCaptureWGC>>;
#elif defined(__linux__)

enum CaptureMode { PW, X11_SHM };

using EncodeStreamTypes = std::variant<EncodeStream<ScreenCapturePW>>;
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
    StreamID AddStream(ID3D11Device* D3D11Device,
                       ID3D11DeviceContext* D3D11Context,
                       session* NetSession,
                       DeviceMap TargetID,
                       CaptureMode Mode);
#elif defined(__linux__)
    StreamID AddStream(session* NetSession, DeviceMap TargetID, CaptureMode Mode);
#endif

    void RemoveStream(size_t StreamID);

    void StopAll();
};
