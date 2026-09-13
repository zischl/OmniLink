#pragma once
#if defined(_WIN32)
#include "platform/windows/D3D11Renderer.hpp"
#include "platform/windows/D3D11RendererCore.hpp"
#elif defined(__linux__)
#include "platform/linux/VkRenderer.hpp"
#endif
