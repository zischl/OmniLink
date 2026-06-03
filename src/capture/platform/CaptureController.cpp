#include "CaptureController.h"
#include "RendererCore.h"
#include "nvenc.h"

void EncodeStream::Start(ID3D11Device* D3D11Device,
                         ID3D11DeviceContext* D3D11Context,
                         session* NetSession,
                         DeviceMap TargetID,
                         CaptureMode Mode,
                         NVENCODER* NvEncodeAPI)
{
    if (Status)
        return;

    Target = NetSession;
    TargetDevice = TargetID;

    if (Mode == CaptureMode::WGC) {
        WGSCapture = new WGScreenCapture(D3D11Device, D3D11Context);
        WGSCapture->CreateWGCBuffer(D3D11Device, &CaptureBuffer);
        WGSCapture->CreateMonitorCapSession(CaptureBuffer, 1920, 1080);
        WGSCapture->StartSession();

        Encoder =
            new NvencSession(D3D11Device, NvEncodeAPI->NVFunctions, CaptureBuffer, 1920, 1080);

        Worker.StartSpinThread(EncodeStream::WGCapSend, Target, WGSCapture, Encoder);
    } else if (Mode == CaptureMode::DXGI) {
        DXGISCapture = new DXGICapture();
        DXGISCapture->InitDXGI(D3D11Device);
        CaptureBuffer = DXGISCapture->GetBuffer();

        Encoder =
            new NvencSession(D3D11Device, NvEncodeAPI->NVFunctions, CaptureBuffer, 1920, 1080);

        Worker.StartSpinThread(EncodeStream::DXGICapSend, Target, DXGISCapture, Encoder);
    }

    Status = true;
}

void EncodeStream::Stop()
{
    if (!Status)
        return;

    Worker.EndLoopedThread();

    if (WGSCapture != nullptr) {
        WGSCapture->CloseSession();
        delete WGSCapture;
        WGSCapture = nullptr;
    }

    if (DXGISCapture != nullptr) {
        delete DXGISCapture;
        DXGISCapture = nullptr;
    }

    if (Encoder != nullptr) {
        delete Encoder;
        Encoder = nullptr;
    }

    CaptureBuffer = nullptr;
    Status = false;
}

CaptureController::StreamID CaptureController::AddStream(ID3D11Device* D3D11Device,
                                                         ID3D11DeviceContext* D3D11Context,
                                                         session* NetSession,
                                                         DeviceMap TargetID,
                                                         CaptureMode Mode)
{

    StreamID id = StreamCount++;
    Streams[StreamCount++].Start(
        D3D11Device, D3D11Context, NetSession, TargetID, Mode, &NvEncodeAPI);
    return id;
}

void CaptureController::RemoveStream(size_t StreamID)
{
    auto iter = Streams.find(static_cast<uint8_t>(StreamID));

    if (iter != Streams.end()) {
        iter->second.Stop();
        Streams.erase(iter);
        StreamCount--;
    }
}

void CaptureController::StopAll()
{
    for (auto& [id, stream] : Streams) {
        stream.Stop();
    }
    Streams.clear();
}
