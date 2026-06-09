#pragma once
#if defined(_WIN32)
#include "platform/windows/D3D11Renderer.h"
#include "platform/windows/D3D11RendererCore.h"
#elif defined(__linux__)
#include "platform/linux/VkRenderer.h"
#endif
