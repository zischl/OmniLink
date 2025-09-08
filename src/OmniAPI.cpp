#include "OmniLink.h"
#include "OmniAPI.h"


void OmniAPI::Ignite(OmniLink& OmniLinkInstance) {
	App = &OmniLinkInstance;
}

void OmniAPI::SwapDeviceLayout() {
	
	FuncArgTypes args = ArraySwapLayout{ 0, 1 };

	App->PushCommandWArgs(args);
	App->ExecuteCommandQueueWArgs();
}

void OmniAPI::Scan() {
	App->PushCommand(ScanInstances);
	App->ExecuteCommandQueue();
}


