#include <stdint.h>
#include <string.h>
#include <opencv2/opencv.hpp>
#include <opencv2/gpu/gpu.hpp>

void bayer10toRGB(unsigned int width, unsigned int height, void *input, void *output)
{
	const unsigned int nInput = width * height * 2;
	const unsigned int nOutput = width * height * 3;
	// Allocate device memory
	void *dInput = 0, *dOutput = 0;
	cudaMalloc(&dInput, nInput);
	cudaMalloc(&dOutput, nOutput);

	// Copy input data to device
	cudaMemcpy(dInput, input, nInput, cudaMemcpyHostToDevice);

	// Execute the kernel
	//gpuBayer10toRGB<<<?, dim3()>>>(dInput, dOutput);

	// Copy output data from device
	cudaMemcpy(output, dOutput, nOutput, cudaMemcpyDeviceToHost);
	//memcpy(output, input, width * height * 3);

	cudaFree(dInput);
	cudaFree(dOutput);
}
