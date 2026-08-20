#include "OmniLink.h"
#include "ClipBoardLink.h"
#include "NetVariance.h"
#include "OmniEnums.h"
#include "OmniPackets.h"
#include <cstdint>

static void HandleFrame(std::vector<StreamWindow*>* Windows, CHAR* Buffer, DWORD BufferSize)
{
    OmniNet::OmniHeader* Header = reinterpret_cast<OmniNet::OmniHeader*>((Buffer + BufferSize - 3));
    if (Windows && Header->Target < Windows->size()) {
        StreamWindow* Target = Windows->at(Header->Target);
        if (Target) {
            Target->SetBufferData(Buffer, BufferSize);
            Target->SetRenderEvent();
        }
    }
}

static void HandleCommand(CHAR* Buffer, DWORD BufferSize, DeviceMap DeviceID)
{
    OmniNet::OmniHeader* Header = reinterpret_cast<OmniNet::OmniHeader*>((Buffer + BufferSize - 3));
    if (Header->Flags == OmniNet::VoidArg) {
        OmniAPI::ExecuteNetCommand(*reinterpret_cast<CoreCommands*>(Buffer));
    } else {

        ByteStreamReader Reader{
            static_cast<uint32_t>(BufferSize - 3), reinterpret_cast<uint8_t*>(Buffer)
        };

        OmniNetCommand Payload = OmniNetCommand::Deserialize(Reader);

        if (!OmniAPI::VerifyCommandToken(DeviceID, Payload)) {
            return;
        }

        OmniCommand command{Payload};

        NetVariantDeserializer(
            command.Args,
            command.ArgTypeIndex,
            std::make_index_sequence<std::variant_size_v<FuncArgTypes>>(),
            Payload.Args.data(),
            Payload.Args.size()
        );

        OmniAPI::ExecuteNetCommandWArgs(command);
    }
}

static void HandleInput(CHAR* Buffer)
{
    INPUT* Payload = reinterpret_cast<INPUT*>(Buffer);
    OmniSynth::ProcInput(*Payload);
}

static void HandleClipboard(CHAR* Buffer, uint32_t BufferSize)
{
    if (!Buffer || BufferSize <= 3)
        return;

    std::string Text(Buffer, BufferSize - 3);
    ClipBoardLink::SetClipTypeText(Text);
}

void NetworkPacketHandler(char* Buffer, uint32_t BufferSize, uint8_t BufferHeader, void* Context)
{
    OmniNet::SessionPacketContext* SessionCtx =
        reinterpret_cast<OmniNet::SessionPacketContext*>(Context);
    std::vector<StreamWindow*>* WindowContext =
        reinterpret_cast<std::vector<StreamWindow*>*>(SessionCtx->UserContext);
    DeviceMap DeviceID = static_cast<DeviceMap>(SessionCtx->UniqueKey);

    switch (BufferHeader) {
    case OmniNet::PacketType::ChunkEnd:
        break;
    case OmniNet::Command: {
        HandleCommand(Buffer, BufferSize, DeviceID);
        break;
    }
    case OmniNet::PacketType::ProcMouse:
    case OmniNet::PacketType::ProcKey: {
        break;
    }
    case OmniNet::PacketType::ProcClipboard: {
        HandleClipboard(Buffer, BufferSize);
        break;
    }
    }
}

void OmniLink::OmniMain()
{
    OmniAPI::Ignite(*this);

    Logger::log("Event Handler Setup Complete");

    OmniMainLoop();
}

void OmniLink::OmniMainLoop()
{
    while (true) {
    }
}

void OmniLink::InitTrayIcon() {}

extern IMGUI_IMPL_API LRESULT
ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
