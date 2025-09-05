#include "OmniLink.h"
#include "OmniAPI.h"


void OmniAPI::Ignite(OmniLink& OmniLinkInstance) {
	App = &OmniLinkInstance;
}

void OmniAPI::test() {
	App->PushCommand(SwapInstanceLayout);
}


