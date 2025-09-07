#include "OmniLink.h"
#include "OmniAPI.h"


void OmniAPI::Ignite(OmniLink& OmniLinkInstance) {
	App = &OmniLinkInstance;
}

void OmniAPI::SwapDeviceLayout() {
	
	ArraySwapLayout args = { 0, 1 };
	Command<FuncArgTypes> command(ScanInstances, args);

	App->PushCommandWArgs(command);
}

void OmniAPI::Scan() {
	App->PushCommand(ScanInstances);
}


