#ifndef OMNILINK_H
#define OMNILINK_H

#pragma once
#include "resource.h"

#include "OmniCore.h"
#include "OmniGUI.h"
#include "SystemLink.h"

class OmniLink : public OmniCore
{
  private:
    OmniGUI* GUI = nullptr;

    std::chrono::steady_clock::duration FrameTimeLimit = std::chrono::nanoseconds(15 * 1000000);

    std::chrono::time_point<std::chrono::steady_clock> LastFrameTime =
        std::chrono::steady_clock::now();

    void OmniMainLoop();

    void InitTrayIcon();

  public:
    OmniLink();

    void OmniMain();
};

#endif
