#include "OmniLink.h"
#include "NetVariance.h"
#include "OmniEnums.h"
#include "OmniPackets.h"
#include <cstdint>

static void HandleFrame(std::vector<StreamWindow*>* Windows, CHAR* Buffer, DWORD BufferSize)
{
    OmniNet::OmniHeader* header = reinterpret_cast<OmniNet::OmniHeader*>((Buffer + BufferSize - 3));
    StreamWindow* target = Windows->at(header->Target);
    target->SetBufferData(Buffer, BufferSize);
    target->SetRenderEvent();
}

static void HandleCommand(CHAR* Buffer, DWORD BufferSize)
{
    OmniNet::OmniHeader* header = reinterpret_cast<OmniNet::OmniHeader*>((Buffer + BufferSize - 3));
    if (header->Flags == OmniNet::VoidArg) {
        OmniAPI::ExecuteNetCommand(*reinterpret_cast<CoreCommands*>(Buffer));
    } else {

        ByteStreamReader Reader{
            static_cast<uint32_t>(BufferSize - 3), reinterpret_cast<uint8_t*>(Buffer)
        };

        OmniNetCommand Payload = OmniNetCommand::Deserialize(Reader);

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

void NetworkPacketHandler(char* Buffer, uint32_t BufferSize, uint8_t BufferHeader, void* Context)
{
    std::vector<StreamWindow*>* WinContext = reinterpret_cast<std::vector<StreamWindow*>*>(Context);

    switch (BufferHeader) {
    case OmniNet::PacketType::ChunkEnd:
        break;
    case OmniNet::Command: {
        break;
    }
    case OmniNet::PacketType::ProcMouse:
    case OmniNet::PacketType::ProcKey: {
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
