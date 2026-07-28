#include "CaptureController.h"
#include "Capture.h"
#include "nvenc.h"
#include <utility>

#if defined(_WIN32)
OmniStreamController::StreamID OmniStreamController::AddStream(
    ID3D11Device*        D3D11Device,
    ID3D11DeviceContext* D3D11Context,
    OmniNetSubStream*    SubStream,
    DeviceMap            TargetID,
    CaptureMode          Mode,
    const StreamConfig&  Config
)
{
    StreamID id = StreamCount++;

    if (Mode == CaptureMode::WGC) {
        ScreenCaptureWGC* WGSCapture = new ScreenCaptureWGC(D3D11Device);

        BufferedNvencSession<CachedPoolNvencSession>* Encoder =
            new BufferedNvencSession<CachedPoolNvencSession>(
                D3D11Device, NvEncodeAPI.NVFunctions, Config.Width, Config.Height, 3
            );

        Streams.try_emplace(
            id,
            std::in_place_type<
                EncodeStream<ScreenCaptureWGC, BufferedNvencSession<CachedPoolNvencSession>, OmniNetSubStream>>
        );
        std::get<EncodeStream<ScreenCaptureWGC, BufferedNvencSession<CachedPoolNvencSession>, OmniNetSubStream>>(
            Streams[id]
        )
            .Start(WGSCapture, Encoder, SubStream, TargetID, Config);

    } else if (Mode == CaptureMode::DXGI) {
        ScreenCaptureDXGI* DXGISCapture = new ScreenCaptureDXGI();
        DXGISCapture->InitDXGI(D3D11Device);
        ID3D11Texture2D* CaptureBuffer = DXGISCapture->GetBuffer();

        BufferedNvencSession<StaticNvencSession>* Encoder =
            new BufferedNvencSession<StaticNvencSession>(
                D3D11Device, NvEncodeAPI.NVFunctions, CaptureBuffer, Config.Width, Config.Height
            );

        Streams.try_emplace(
            id,
            std::in_place_type<
                EncodeStream<ScreenCaptureDXGI, BufferedNvencSession<StaticNvencSession>, OmniNetSubStream>>
        );
        std::get<EncodeStream<ScreenCaptureDXGI, BufferedNvencSession<StaticNvencSession>, OmniNetSubStream>>(
            Streams[id]
        )
            .Start(DXGISCapture, Encoder, SubStream, TargetID, Config);
    }

    return id;
}
#elif defined(__linux__)
StreamID OmniStreamController::AddStream(
    OmniNetSubStream* SubStream, DeviceMap TargetID, CaptureMode Mode, const StreamConfig& Config
)
{
    StreamID id = StreamCount++;

    return id;
}
#endif

void OmniStreamController::RemoveStream(size_t StreamID)
{
    auto iter = Streams.find(StreamID);

    if (iter != Streams.end()) {
        std::visit(
            [](auto& stream) {
                stream.Stop();
                // might have to handle cleanup later properly
                if (stream.Source)
                    delete stream.Source;
                if (stream.Encoder)
                    delete stream.Encoder;
            },
            iter->second
        );
        Streams.erase(iter);
    }
}

void OmniStreamController::StopAll()
{
    for (auto& [id, variant_stream] : Streams) {
        std::visit(
            [](auto& stream) {
                stream.Stop();
                if (stream.Source)
                    delete stream.Source;
                if (stream.Encoder)
                    delete stream.Encoder;
            },
            variant_stream
        );
    }
    Streams.clear();
    StreamCount = 0;
}
