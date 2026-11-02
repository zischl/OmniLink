#include "SystemLink.h"
#include "OmniLogger.hpp"

OmniSystemLink::OmniSystemLink(OmniGraphicsContext& GraphicsContext)
    : GraphicsContext(GraphicsContext)
{
}

OmniSystemLink::~OmniSystemLink() {}

void OmniSystemLink::SetupSystemLink(HINSTANCE hInstance_, int nCmdShow_, HWND WindowID_)
{
    hInstance                           = hInstance_;
    nCmdShow                            = nCmdShow_;
    WindowID                            = WindowID_;
    ClipboardLinkModule.ActiveInstances = ActiveInstances;
    InputLinkModule.Setup(WindowID_);
    OmniRouter.SetResolution(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    Logger::log(
        "SetupSystemLink: Initialized OmniRouter resolution to {}x{}",
        GetSystemMetrics(SM_CXSCREEN),
        GetSystemMetrics(SM_CYSCREEN)
    );
}
