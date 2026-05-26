#ifndef KERNELCAST_H
#define KERNELCAST_H


#pragma once
#include <cuda_runtime.h>
#include <device_launch_parameters.h>

#pragma comment(lib, "cuda.lib")

__global__ void CastNV12toBGRA(const uint8_t* NV12Buffer, int width, int height, cudaSurfaceObject_t OutputSurface, int pitch);

void Cast2BGRA(const uint8_t* NV12Buffer, int width, int height, cudaSurfaceObject_t OutputSurface, int pitch);

#endif