#include "OmniAPI.h"
#include "OmniLink.h"
#include "OmniPackets.h"

void OmniAPI::Ignite(OmniLink& OmniLinkInstance)
{
    App = &OmniLinkInstance;
}

void OmniAPI::SwapDeviceLayout(uint8_t index1, uint8_t index2)
{

    FuncArgTypes args = ArraySwapLayout{index1, index2};

    App->PushCommandWArgs(args);
    App->NotifyCommandQueue();
}

void OmniAPI::Scan()
{
    App->PushCommand(ScanInstances);
    App->NotifyCommandQueue();
}

void OmniAPI::Connect(ConnectionRequest Request)
{
    FuncArgTypes args = ConnectionRequest(Request);
    App->PushCommandWArgs(args);
    App->NotifyCommandQueue();
}

void OmniAPI::AcceptHandshake(DeviceMap DeviceID, bool trustPermanently)
{
    FuncArgTypes args =
        HandshakeResponse{DeviceID, HandshakeResponse::Action::ACCEPT, trustPermanently};
    App->PushCommandWArgs(args);
    App->NotifyCommandQueue();
}

void OmniAPI::RejectHandshake(DeviceMap DeviceID)
{
    FuncArgTypes args = HandshakeResponse{DeviceID, HandshakeResponse::Action::REJECT};
    App->PushCommandWArgs(args);
    App->NotifyCommandQueue();
}

void OmniAPI::CancelHandshake(DeviceMap DeviceID)
{
    FuncArgTypes args = HandshakeResponse{DeviceID, HandshakeResponse::Action::CANCEL};
    App->PushCommandWArgs(args);
    App->NotifyCommandQueue();
};

void OmniAPI::ExecuteNetCommand(CoreCommands Command)
{
    App->PushCommand(Command);
    App->NotifyCommandQueue();
}

void OmniAPI::ExecuteNetCommandWArgs(OmniCommand Command)
{
    App->PushCommandWArgs(Command.Args);
    App->NotifyCommandQueue();
}

void OmniAPI::ToggleFeature(FeatureTypes FeatureIndex, DeviceMap Index)
{
    App->ToggleFeature(FeatureIndex, Index);
}

void OmniAPI::Get(DataTypes)
{
    App;
}
