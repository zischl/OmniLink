#include "OmniLink.h"
#include "NetVariance.h"
#include "OmniEnums.h"
#include "OmniPackets.h"
#include "SessionManager.h"

#include <memory>

namespace {

static void HandleFrame(std::vector<LinForge*>* Windows, CHAR* Buffer, DWORD BufferSize)
{
    // zeroth window since i'm still implenting multi window creation
    OmniNet::OmniHeader* header = reinterpret_cast<OmniNet::OmniHeader*>((Buffer + BufferSize - 3));
    WinForge* target = Windows->at(header->Target);
    target->SetBufferData(Buffer, BufferSize);
    target->SetRenderEvent();
}

static void HandleCommand(CHAR* Buffer, DWORD BufferSize)
{
    OmniNet::OmniHeader* header = reinterpret_cast<OmniNet::OmniHeader*>((Buffer + BufferSize - 3));
    if (header->Flags == OmniNet::VoidArg) {
        OmniAPI::ExecuteNetCommand(*reinterpret_cast<CoreCommands*>(Buffer));
    } else {

        ByteStreamReader Reader{static_cast<uint32_t>(BufferSize - 3),
                                reinterpret_cast<uint8_t*>(Buffer)};

        OmniNetCommand Payload = OmniNetCommand::Deserialize(Reader);

        OmniCommand command{Payload};

        NetVariantDeserializer(command.Args,
                               command.ArgTypeIndex,
                               std::make_index_sequence<std::variant_size_v<FuncArgTypes>>(),
                               Payload.Args.data(),
                               Payload.Args.size());

        OmniAPI::ExecuteNetCommandWArgs(command);
    }
}

static void HandleInput(CHAR* Buffer)
{
    INPUT* Payload = reinterpret_cast<INPUT*>(Buffer);
    OmniSynth::ProcInput(*Payload);
}

static void
NetworkPacketHandler(CHAR* Buffer, DWORD BufferSize, uint8_t BufferHeader, void* Context)
{
    std::vector<WinForge*>* WinContext = reinterpret_cast<std::vector<WinForge*>*>(Context);

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
};

} // namespace

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

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,
                                                             UINT msg,
                                                             WPARAM wParam,
                                                             LPARAM lParam);
