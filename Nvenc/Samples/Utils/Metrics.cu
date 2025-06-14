/*
 * This copyright notice applies to this header file only:
 *
 * Copyright (c) 2010-2024 NVIDIA Corporation
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the software, and to permit persons to whom the
 * software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "Metrics.h"      

__global__ void squaredErrorKernel(const uint8_t* image1, const uint8_t* image2, uint32_t width, uint32_t height, uint32_t pitch, float* sse) {
    extern __shared__ float sdata[];

    unsigned int tid = threadIdx.x;
    unsigned int idx = blockIdx.x * blockDim.x + threadIdx.x;
    unsigned int stride = gridDim.x * blockDim.x;

    float sum = 0.0f;

    // Accumulate squared error for this thread's part of the image
    while (idx < width * height) {
        int row = idx / width;
        int col = idx % width;
        
        int idx1 = row * pitch + col;
        int idx2 = row * pitch + col;

        float diff = static_cast<float>(image1[idx1]) - static_cast<float>(image2[idx2]);
        sum += diff * diff;
        idx += stride;
    }

    // Store the result in shared memory
    sdata[tid] = sum;
    __syncthreads();

    // Reduce within the block
    for (unsigned int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            sdata[tid] += sdata[tid + s];
        }
        __syncthreads();
    }

    // Write the result of this block to the output array
    if (tid == 0) {
        atomicAdd(sse, sdata[0]);
    }
}

void squaredError(uint8_t* image1, uint8_t* image2, uint32_t width, uint32_t height, uint32_t pitch, float &sse) {
    int threadsPerBlock = 256;
    int blocksPerGrid = (width * height + threadsPerBlock - 1) / threadsPerBlock;
    int sharedMemSize = threadsPerBlock * sizeof(float);
    float *sseDevice;

    ck(cudaMalloc(&sseDevice, sizeof(float)));
    ck(cudaMemset(sseDevice, 0, sizeof(float)));

    squaredErrorKernel<<<blocksPerGrid, threadsPerBlock, sharedMemSize>>>(image1, image2, width, height, pitch, sseDevice);
    CudaCheckError();
    ck(cudaDeviceSynchronize());

    ck(cudaMemcpy(&sse, sseDevice, sizeof(float), cudaMemcpyDeviceToHost));
    ck(cudaFree(sseDevice));
}

void calcPSNRY(uint8_t* ref, uint8_t* dis, uint32_t width, uint32_t height, uint32_t pitch, float &psnr) { 
    float sse = 0.0f;
    float mse = 0.0f;
    uint32_t MAX = 255;

    squaredError(ref, dis, width, height, pitch, sse);
    mse = sse / (width * height);
    if (mse) 
        psnr = 10.0f * log10f(MAX * MAX / mse);
    else
        psnr = 100.0;

}