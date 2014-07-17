#include "cuMatrix.h"

__global__ void make_BlackWhite(int *image, int N){
	unsigned int x = blockIdx.x * blockDim.x + threadIdx.x;
	unsigned int y = blockIdx.y * blockDim.y + threadIdx.y;

	image[y*N + x] = image[y*N + x] > 128 ? 255 : 0;
}

void convertToArray(int **matrix, int *array, int N){
	for(unsigned int i=0; i< N; i++)
		for(unsigned int j=0; j< N; j++)
			array[i*N+ j] = matrix[i][j];
}
