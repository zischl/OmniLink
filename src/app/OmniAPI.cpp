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
    App->PushCommandWArgs(DeviceMap::C0, args);
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
    App->PushCommandWArgs(DeviceMap::C0, args);
    App->NotifyCommandQueue();
}

const std::vector<OmniInstanceGroup>& OmniAPI::GetInstanceGroups()
{
    static const std::vector<OmniInstanceGroup> EmptyGroups{};
    return App ? App->GetInstanceGroups() : EmptyGroups;
}

void OmniAPI::SaveCurrentGroup(const char* name, const char* subtitle)
{
    if (App) {
        App->SaveCurrentGroup(name, subtitle);
    }
}

void OmniAPI::ConnectGroup(size_t index)
{
    if (App) {
        App->ConnectGroup(index);
    }
}

void OmniAPI::RemoveInstanceGroup(size_t index)
{
    if (App) {
        App->RemoveInstanceGroup(index);
    }
}

const OmniQrypt* OmniAPI::GetQryptManager()
{
    return App ? App->GetQryptManager() : nullptr;
}

void OmniAPI::ForgetDevice(DeviceMap DeviceID)
{
    if (App) {
        App->ForgetDevice(DeviceID);
    }
}

void OmniAPI::AcceptHandshake(DeviceMap DeviceID, bool trustPermanently)
{
    FuncArgTypes args =
        HandshakeResponse{DeviceID, HandshakeResponse::Action::ACCEPT, trustPermanently};
    App->PushCommandWArgs(DeviceMap::C0, args);
    App->NotifyCommandQueue();
}

void OmniAPI::RejectHandshake(DeviceMap DeviceID)
{
    FuncArgTypes args = HandshakeResponse{DeviceID, HandshakeResponse::Action::REJECT};
    App->PushCommandWArgs(DeviceMap::C0, args);
    App->NotifyCommandQueue();
}

void OmniAPI::CancelHandshake(DeviceMap DeviceID)
{
    FuncArgTypes args = HandshakeResponse{DeviceID, HandshakeResponse::Action::CANCEL};
    App->PushCommandWArgs(DeviceMap::C0, args);
    App->NotifyCommandQueue();
}

void OmniAPI::ExecuteNetCommand(CoreCommands Command)
{
    App->PushCommand(Command);
    App->NotifyCommandQueue();
}

void OmniAPI::ExecuteNetCommandWArgs(OmniCommand Command)
{
    App->PushCommandWArgs(Command.DeviceID, Command.Args);
    App->NotifyCommandQueue();
}

bool OmniAPI::VerifyCommandToken(DeviceMap DeviceID, const OmniNetCommand& Command)
{
    return App ? App->VerifyCommandToken(DeviceID, Command) : false;
}

void OmniAPI::ToggleFeature(FeatureTypes FeatureIndex, DeviceMap Index)
{
    App->ToggleFeature(FeatureIndex, Index);
}

void OmniAPI::Get(DataTypes)
{
    App;
}
