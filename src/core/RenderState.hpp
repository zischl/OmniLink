#ifndef RENDERSTATE_H
#define RENDERSTATE_H

#pragma once

#if defined(_WIN32)
#include <d3d11.h>
#include <dxgi1_5.h>
#elif defined(__linux__)
#include <vulkan/vulkan.h>
#endif

struct OmniRenderState
{
#if defined(_WIN32)
    ID3D11Device* Device = nullptr;
    ID3D11DeviceContext* Context = nullptr;
    IDXGISwapChain3* Swapchain = nullptr;
    ID3D11RenderTargetView* RTV = nullptr;
#elif defined(__linux__)
    VkDevice Device = VK_NULL_HANDLE;
    VkQueue Queue = VK_NULL_HANDLE;
    VkSwapchainKHR Swapchain = VK_NULL_HANDLE;
#endif
};

#endif
