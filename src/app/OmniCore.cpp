#include "OmniCore.h"
#include "Helper.h"
#include "OmniEnums.h"
#include "OmniPackets.h"
#include "OmniTypes.h"
#include <vector>

DeviceMap OmniCore::ActiveIOProcTarget = DeviceMap::C0;
DeviceMap OmniCore::SelectedTargetDevice = DeviceMap::C0;

OmniCore::OmniCore() = default;

void OmniCore::ScanInstances()
{
    InstanceRegistry.RefreshInstanceList([this]() -> void { UIState = OmniGUIState::RENDER; });
}

void OmniCore::ConnectInstance(DeviceMap DeviceID)
{
    if (!SystemLink.networkPacketHandler) {
        return;
    }

    ConnectionRequest Request{DeviceID, "OMNILINK"};
    std::unique_ptr<session> NetSession =
        SessionManager.Connect(Request,
                               InstanceRegistry.UserInstance,
                               InstanceRegistry.ActiveInstances[DeviceID],
                               SystemLink.networkPacketHandler,
                               &ActiveWindows);

    InstanceRegistry.ActivateInstance(DeviceID, std::move(NetSession));

    SystemLink.IOCapture.AddEdgeCondition(DeviceID);

    std::vector<uint8_t> RequestBytes = ConnectionRequest::Serialize(Request);

    OmniNetCommand Command{
        CoreCommandsWArgs::ConnectDevice,
        static_cast<uint32_t>(Variance::GetVariantTypeIndex<ConnectionRequest, FuncArgTypes>),
        RequestBytes};
    TransmitNetCommand(DeviceID, Command, 0, OmniNet::Argonized);
}

void OmniCore::SwapInstanceLayout(int DeviceID1, int DeviceID2)
{
    InstanceRegistry.SwapInstances(DeviceMap(DeviceID1), DeviceMap(DeviceID2));
}

void OmniCore::CreateStreamLink(WindowCreationData& WindowInfo)
{
    SystemLink.CreateStreamWindow(WindowInfo);
}

void OmniCore::ToggleFeature(FeatureTypes FeatureIndex, DeviceMap Index)
{

    switch (FeatureIndex) {
    case FeatureTypes::ScreenLink: {
        WindowCreationData WGC{"Test Window"};

        OmniNetCommand command{};
        command.CommandType = CoreCommandsWArgs::CreateStreamLink;
        command.ArgTypeIndex = 2;

        std::vector<uint8_t> payload = WindowCreationData::Serialize(WGC);
        command.Args = payload;
        command.ArgArrayLength = payload.size();

        TransmitNetCommand(Index, command, 0, OmniNet::Argonized);

        SystemLink.AddCaptureStream(InstanceRegistry.ActiveInstances[Index].InstanceSession.get(),
                                    Index,
                                    CaptureMode::DXGI);

        break;
    }
    case FeatureTypes::WindowLink: {
        WindowCreationData WGC{"Test Window"};

        OmniNetCommand command{};
        command.CommandType = CoreCommandsWArgs::CreateStreamLink;
        command.ArgTypeIndex = 2;

        std::vector<uint8_t> payload = WindowCreationData::Serialize(WGC);
        command.Args = payload;
        command.ArgArrayLength = payload.size();

        TransmitNetCommand(Index, command, 0, OmniNet::Argonized);

        SystemLink.AddCaptureStream(
            InstanceRegistry.ActiveInstances[Index].InstanceSession.get(), Index, CaptureMode::WGC);

        break;
    }

    case FeatureTypes::InputLink:
        SystemLink.ToggleEdgeProbe(InstanceRegistry.ActiveInstances);
        SystemLink.SyncInputFilter();
        break;

    case FeatureTypes::AudioLink: {
        break;
    }
    }
}
