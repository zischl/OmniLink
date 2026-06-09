#include "CaptureController.h"
#include "Capture.h"
#include "nvenc.h"
#include <utility>

#if defined(_WIN32)
OmniStreamController::StreamID OmniStreamController::AddStream(ID3D11Device* D3D11Device,
                                                               ID3D11DeviceContext* D3D11Context,
                                                               session* NetSession,
                                                               DeviceMap TargetID,
                                                               CaptureMode Mode)
{
    StreamID id = StreamCount++;

    if (Mode == CaptureMode::WGC) {
        ScreenCaptureWGC* WGSCapture = new ScreenCaptureWGC(D3D11Device, D3D11Context);
        ID3D11Texture2D* CaptureBuffer = nullptr;
        WGSCapture->CreateWGCBuffer(D3D11Device, &CaptureBuffer);
        WGSCapture->CreateMonitorCapSession(CaptureBuffer, 1920, 1080);
        WGSCapture->StartSession();

        NvencSession* Encoder =
            new NvencSession(D3D11Device, NvEncodeAPI.NVFunctions, CaptureBuffer, 1920, 1080);

        Streams.try_emplace(id, std::in_place_type<EncodeStream<ScreenCaptureWGC>>);
        std::get<EncodeStream<ScreenCaptureWGC>>(Streams[id])
            .Start(WGSCapture, Encoder, NetSession, TargetID);

    } else if (Mode == CaptureMode::DXGI) {
        ScreenCaptureDXGI* DXGISCapture = new ScreenCaptureDXGI();
        DXGISCapture->InitDXGI(D3D11Device);
        ID3D11Texture2D* CaptureBuffer = DXGISCapture->GetBuffer();

        NvencSession* Encoder =
            new NvencSession(D3D11Device, NvEncodeAPI.NVFunctions, CaptureBuffer, 1920, 1080);

        Streams.try_emplace(id, std::in_place_type<EncodeStream<ScreenCaptureDXGI>>);
        std::get<EncodeStream<ScreenCaptureDXGI>>(Streams[id])
            .Start(DXGISCapture, Encoder, NetSession, TargetID);
    }

    return id;
}
#elif defined(__linux__)
StreamID CaptureController::AddStream(session* NetSession, DeviceMap TargetID, CaptureMode Mode)
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
            iter->second);
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
            variant_stream);
    }
    Streams.clear();
    StreamCount = 0;
}
