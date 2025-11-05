#include "OmniLink.h"
#include "OmniAPI.h"


void OmniAPI::Ignite(OmniLink& OmniLinkInstance) {
	App = &OmniLinkInstance;
}

void OmniAPI::SwapDeviceLayout(uint8_t index1, uint8_t index2) {
	
	FuncArgTypes args = ArraySwapLayout{ index1, index2};

	App->PushCommandWArgs(args);
	App->ExecuteCommandQueueWArgs();
}

void OmniAPI::Scan() 
{
	App->PushCommand(ScanInstances);
	App->ExecuteCommandQueue();
}

void OmniAPI::Connect(DeviceMap DevMapIDx)
{
	FuncArgTypes args = DeviceMap(DevMapIDx);
	App->PushCommandWArgs(args);
	App->ExecuteCommandQueueWArgs();
}

void OmniAPI::ExecuteNetCommand(CoreCommands Command)
{
	App->PushCommand(Command);
	App->ExecuteCommandQueue();
}

void OmniAPI::ExecuteNetCommandWArgs(OmniCommand Command)
{
	App->PushCommandWArgs(Command.Args);
	App->ExecuteCommandQueueWArgs();
}

