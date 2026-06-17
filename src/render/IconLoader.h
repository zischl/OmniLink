#pragma once
#if defined(WIN32)
#include "D3D11RendererCore.h"
#elif defined(UNIX)
#include <GL/glew.h>
#endif

class IconLoader
{
  public:
    IconLoader() = default;
    ~IconLoader() { Release(); }

    bool LoadEmbeddedRGBA(const unsigned char* bytes,
                          unsigned int width,
                          unsigned int height,
                          ID3D11Device* D3DDevice);
    void Release();

    void* GetTextureID() const { return IconTextureHandle; }

  private:
    void* IconTextureHandle = nullptr;
};
