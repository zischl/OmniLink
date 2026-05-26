#include "CuKernel.cuh"


__global__ void CastNV12toBGRA(const uint8_t* NV12Buffer, int width, int height, cudaSurfaceObject_t OutputSurface, int pitch) {
    int x = threadIdx.x + blockIdx.x * blockDim.x;
    int y = threadIdx.y + blockIdx.y * blockDim.y;

    if (x >= width || y >= height)
        return;

    int pixelIndex = x + (y * pitch);
    float _Y = (float)(NV12Buffer[pixelIndex]) - 16.0f;
    float _U = (float)(NV12Buffer[(height * pitch) + ((y/2) * pitch) + (x & ~1)]) - 128.0f;
    float _V = (float)(NV12Buffer[(height * pitch) + ((y/2) * pitch) + (x | 1)]) - 128.0f;

    uint8_t  R = min(max(int((_V * 1.5748) + _Y + 0.5f), 0), 255);
    uint8_t  G = min(max(int(_Y - (0.1873 * _U) - (0.4681 * _V) + 0.5f), 0), 255);
    uint8_t  B = min(max(int((_U * 1.8556) + _Y + 0.5f), 0), 255);

    uchar4 PixelBGRA = make_uchar4(B, G, R, 255);

    surf2Dwrite(PixelBGRA, OutputSurface, x*4, y, cudaBoundaryModeTrap);

}

void Cast2BGRA(const uint8_t* NV12Buffer, int width, int height, cudaSurfaceObject_t OutputSurface, int pitch) {
    dim3 threads(16, 16);
    dim3 blocks((width + 15) / 16, (height + 15) / 16);
    CastNV12toBGRA <<<blocks, threads>>> (NV12Buffer, width, height, OutputSurface, pitch);
}

