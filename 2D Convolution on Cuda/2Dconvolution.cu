// Student: Catalin Consantin Usurelu
// Grupa: 333CA

/*
 * Copyright 1993-2006 NVIDIA Corporation.  All rights reserved.
 *
 * NOTICE TO USER:
 *
 * This source code is subject to NVIDIA ownership rights under U.S. and
 * international Copyright laws.
 *
 * This software and the information contained herein is PROPRIETARY and
 * CONFIDENTIAL to NVIDIA and is being provided under the terms and
 * conditions of a Non-Disclosure Agreement.  Any reproduction or
 * disclosure to any third party without the express written consent of
 * NVIDIA is prohibited.
 *
 * NVIDIA MAKES NO REPRESENTATION ABOUT THE SUITABILITY OF THIS SOURCE
 * CODE FOR ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR
 * IMPLIED WARRANTY OF ANY KIND.  NVIDIA DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOURCE CODE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE.
 * IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
 * OR PERFORMANCE OF THIS SOURCE CODE.
 *
 * U.S. Government End Users.  This source code is a "commercial item" as
 * that term is defined at 48 C.F.R. 2.101 (OCT 1995), consisting  of
 * "commercial computer software" and "commercial computer software
 * documentation" as such terms are used in 48 C.F.R. 12.212 (SEPT 1995)
 * and is provided to the U.S. Government only as a commercial end item.
 * Consistent with 48 C.F.R.12.212 and 48 C.F.R. 227.7202-1 through
 * 227.7202-4 (JUNE 1995), all U.S. Government End Users acquire the
 * source code with only those rights set forth herein.
 */

// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <helper_cuda.h>
#include <helper_timer.h>
#include <helper_functions.h>
#include <helper_math.h>

// includes, project
#include "2Dconvolution.h"

#define OFFSET KERNEL_SIZE / 2
#define ALLIGN_MID KERNEL_SIZE / 2


////////////////////////////////////////////////////////////////////////////////
// declarations, forward

extern "C"
void computeGold(float*, const float*, const float*, unsigned int, unsigned int);

Matrix AllocateDeviceMatrix(int width, int height);
Matrix AllocateMatrix(int width, int height);
void FreeDeviceMatrix(Matrix* M);
void FreeMatrix(Matrix* M);

void ConvolutionOnDevice(const Matrix M, const Matrix N, Matrix P);
void ConvolutionOnDeviceShared(const Matrix M, const Matrix N, Matrix P);

// Get a matrix element
__device__ float GetElement(const Matrix A, int row, int col)
{
  return A.elements[row * A.pitch + col];
}
 
// Set a matrix element
__device__ void SetElement(Matrix A, int row, int col, float value)
{
  A.elements[row * A.pitch + col] = value;
}

////////////////////////////////////////////////////////////////////////////////
// Înmulțirea fără memorie partajată
////////////////////////////////////////////////////////////////////////////////
__global__ void ConvolutionKernel(Matrix M, Matrix N, Matrix P)
{
    // Each thread computes one element of P
    // by accumulating results into Pvalue
    
    float Pvalue = 0;
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;
    
    // Outside of range
    if(row >= N.height || col >= N.width)
    {
        return;
    }
  
    // check the start and end values of m and n to prevent overrunning the 
    // matrix edges
    unsigned int mbegin = (row < 2)? 2 - row : 0;
    unsigned int mend = (row > (N.height - 3))?
                            N.height - row + 2 : 5;
    unsigned int nbegin = (col < 2)? 2 - col : 0;
    unsigned int nend = (col > (N.width - 3))?
                            (N.width - col) + 2 : 5;
                            
    // overlay A over B centered at element (i,j).  For each 
    // overlapping element, multiply the two and accumulate
    for(unsigned int m = mbegin; m < mend; m++)
    {
        for(unsigned int n = nbegin; n < nend; n++)
        {
            Pvalue += M.elements[m * 5 + n] * 
                    N.elements[N.width * (row + m - 2) + (col + n - 2)];
        }
    }
    
    // store the result
    P.elements[row * N.width + col] = (float)Pvalue;
}


////////////////////////////////////////////////////////////////////////////////
// Înmulțirea cu memorie partajată
////////////////////////////////////////////////////////////////////////////////
__global__ void ConvolutionKernelShared(Matrix M, Matrix N, Matrix P)
{
    float Pvalue = 0;
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;

   
    __shared__ float Ms[KERNEL_SIZE * KERNEL_SIZE];
    __shared__ float Ns[BLOCK_SIZE + KERNEL_SIZE - 1][BLOCK_SIZE + KERNEL_SIZE - 1];
   
   // Primul bloc de 5X5 threaduri copiaza matricea shared
   if(threadIdx.x < 5 && threadIdx.y < 5)
        Ms[threadIdx.y * KERNEL_SIZE + threadIdx.x] = M.elements[threadIdx.y * KERNEL_SIZE + threadIdx.x];

   __syncthreads();
   
   // ns_row, ns_col sunt defapt niste indici care repezinta blocul de threaduri
   // relativ la coltul stanga sus al lui Ns (matricea N shared)
   // matricea este centrata in mijloc (adica decalam cu 2) - practic
   // la inceput se suprapune cu blocul pentru care calculam valori
   int ns_row = threadIdx.y + ALLIGN_MID;
   int ns_col = threadIdx.x + ALLIGN_MID;
   
   // Explicatii cod in Readme
   // Aici doar folosim "blocul de threaduri" (sau parti ale lui) pentru a
   // copia parti din N in Ns
   if(row - OFFSET >= 0 && col - OFFSET >= 0)    
        Ns[ns_row - OFFSET][ns_col - OFFSET] = GetElement(N, row - OFFSET, col - OFFSET);
    
    if(threadIdx.y >= BLOCK_SIZE - 4)
    {
        if(row + OFFSET < N.height && col - OFFSET >= 0)    
            Ns[ns_row + OFFSET][ns_col - OFFSET] = GetElement(N, row + OFFSET, col - OFFSET);
            
        if(threadIdx.x >= BLOCK_SIZE - 4)
        {
            if(row + OFFSET < N.height && col + OFFSET < N.width)    
                Ns[ns_row + OFFSET][ns_col + OFFSET] = GetElement(N, row + OFFSET, col + OFFSET);
        }
    }
    
    if(threadIdx.x >= BLOCK_SIZE - 4)
         if(row - OFFSET >= 0 && col + OFFSET < N.width)    
        Ns[ns_row - OFFSET][ns_col + OFFSET] = GetElement(N, row - OFFSET, col + OFFSET);

    // Aveam nevoie de toate thread-urile pentru partea de mai sus (mai usor de implementat)
    if(row >= N.height || col >= N.width)
    {
        return;
    }
    
    // Asteptam sa se termine copierea in Ns
    __syncthreads();

    // Ne intereseaza doar "mijlocul lui Ns", adica fara margini
    // => incepem cu un offset de 2 fata de coltul stanga sus
    // Restul codului este ca la non-shared sau varianta seriala
    // doar ca accesam matricile shared
    int NsRow = threadIdx.y + KERNEL_SIZE / 2;
    int NsCol = threadIdx.x + KERNEL_SIZE / 2;
                    
    // check the start and end values of m and n to prevent overrunning the 
    // matrix edges
    unsigned int mbegin = (row < 2)? 2 - row : 0;
    unsigned int mend = (row > (N.height - 3))?
                            N.height - row + 2 : 5;
    unsigned int nbegin = (col < 2)? 2 - col : 0;
    unsigned int nend = (col > (N.width - 3))?
                            (N.width - col) + 2 : 5;
                            
    // overlay A over B centered at element (i,j).  For each 
    // overlapping element, multiply the two and accumulate
    for(unsigned int m = mbegin; m < mend; m++)
    {
        for(unsigned int n = nbegin; n < nend; n++)
        {
            Pvalue += Ms[m * 5 + n] * 
                    Ns[NsRow + m - 2][NsCol + n - 2];
        }
    }
    
    // store the result
    P.elements[row * N.width + col] = (float)Pvalue;
}

////////////////////////////////////////////////////////////////////////////////
// Returnează 1 dacă matricele sunt ~ egale
////////////////////////////////////////////////////////////////////////////////
int CompareMatrices(Matrix A, Matrix B)
{
    int i;
    if(A.width != B.width || A.height != B.height || A.pitch != B.pitch)
        return 0;
    int size = A.width * A.height;
    for(i = 0; i < size; i++)
        if(fabs(A.elements[i] - B.elements[i]) > MAX_ERR)
            return 0;
    return 1;
}
void GenerateRandomMatrix(Matrix m)
{
    int i;
    int size = m.width * m.height;

    srand(time(NULL));

    for(i = 0; i < size; i++)
        m.elements[i] = rand() / (float)RAND_MAX;
}

////////////////////////////////////////////////////////////////////////////////
// main
////////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv) 
{
    int width = 0, height = 0;
    FILE *f, *out;
    if(argc < 2)
    {
        printf("Argumente prea puține, trimiteți id-ul testului care trebuie rulat\n");
        return 0;
    }
    char name[100];
    sprintf(name, "./tests/test_%s.txt", argv[1]);
    f = fopen(name, "r");
    out = fopen("out.txt", "a");
    fscanf(f, "%d%d", &width, &height);
    Matrix M;//kernel de pe host
    Matrix N;//matrice inițială de pe host
    Matrix P;//rezultat fără memorie partajată calculat pe GPU
    Matrix PS;//rezultatul cu memorie partajată calculat pe GPU
    
    M = AllocateMatrix(KERNEL_SIZE, KERNEL_SIZE);
    N = AllocateMatrix(width, height);        
    P = AllocateMatrix(width, height);
    PS = AllocateMatrix(width, height);

    GenerateRandomMatrix(M);
    GenerateRandomMatrix(N);
    
    printf("Test for matrix size %dX%d = %d\n", height, width, height * width);

    // M * N pe device
    ConvolutionOnDevice(M, N, P);
    
    // M * N pe device cu memorie partajată
    ConvolutionOnDeviceShared(M, N, PS);

    
    //pentru măsurarea timpului de execuție pe CPU
    StopWatchInterface *kernelTime = NULL;
    sdkCreateTimer(&kernelTime);
    sdkResetTimer(&kernelTime);
    
    // calculează rezultatul pe CPU pentru comparație
    Matrix reference = AllocateMatrix(P.width, P.height);
    
    sdkStartTimer(&kernelTime);
    computeGold(reference.elements, M.elements, N.elements, N.height, N.width);
    
    sdkStopTimer(&kernelTime);
    printf ("Timp execuție CPU: %f ms\n", sdkGetTimerValue(&kernelTime));
     
        
    // verifică dacă rezultatul obținut pe device este cel așteptat
    int res = CompareMatrices(reference, P);
    printf("Test global %s\n", (1 == res) ? "PASSED" : "FAILED");
    fprintf(out, "Test global %s %s\n", argv[1], (1 == res) ? "PASSED" : "FAILED");
    
  
     
     

    // verifică dacă rezultatul obținut pe device cu memorie partajată este cel așteptat
    //  int ress = CompareMatrices(reference, PS);
    int ress = CompareMatrices(reference, PS);
    printf("Test shared %s\n", (1 == ress) ? "PASSED" : "FAILED");
    fprintf(out, "Test shared %s %s\n", argv[1], (1 == ress) ? "PASSED" : "FAILED");
    
    printf("\n");
   
    // Free matrices
    FreeMatrix(&M);
    FreeMatrix(&N);
    FreeMatrix(&P);
    FreeMatrix(&PS);

    fclose(f);
    fclose(out);
    return 0;
}


////////////////////////////////////////////////////////////////////////////////
//! Run a simple test for CUDA
////////////////////////////////////////////////////////////////////////////////
void ConvolutionOnDevice(const Matrix M, const Matrix N, Matrix P)
{
    Matrix Md, Nd, Pd; //matricele corespunzătoare de pe device
    size_t size;

    //pentru măsurarea timpului de execuție în kernel
    StopWatchInterface *kernelTime = NULL;
    sdkCreateTimer(&kernelTime);
    sdkResetTimer(&kernelTime);

    Md = AllocateDeviceMatrix(M.width, M.height);
    Nd = AllocateDeviceMatrix(N.width, N.height);
    Pd = AllocateDeviceMatrix(P.width, P.height);

    
    //matrice kernel
    size = M.width * M.height * sizeof(float); 
    cudaMemcpy(Md.elements, M.elements, size, cudaMemcpyHostToDevice);
    
    //matrice inițială de pe host
    size = N.width * N.height * sizeof(float); 
    cudaMemcpy( Nd.elements, N.elements, size, cudaMemcpyHostToDevice);

    // dimGrid: daca nu se imparte perfect o dimensiune, facem ceil() pe rezultat
    // ca sa acoperim toate cazurile
    dim3 dimBlock(BLOCK_SIZE, BLOCK_SIZE);
    dim3 dimGrid((N.width + dimBlock.x - 1) / dimBlock.x, (N.height + dimBlock.y - 1) / dimBlock.y);
    

    sdkStartTimer(&kernelTime);

    ConvolutionKernel<<<dimGrid, dimBlock>>>(Md, Nd, Pd);

    cudaThreadSynchronize();
    sdkStopTimer(&kernelTime);
    printf ("Timp execuție kernel: %f ms\n", sdkGetTimerValue(&kernelTime));
    
    size = P.width * P.height * sizeof(float); 
    cudaMemcpy(P.elements, Pd.elements, size, cudaMemcpyDeviceToHost);
    
    FreeDeviceMatrix(&Md);
    FreeDeviceMatrix(&Nd);
    FreeDeviceMatrix(&Pd);
}


void ConvolutionOnDeviceShared(const Matrix M, const Matrix N, Matrix P)
{
    Matrix Md, Nd, Pd; //matricele corespunzătoare de pe device
    size_t size;

    //pentru măsurarea timpului de execuție în kernel
    StopWatchInterface *kernelTime = NULL;
    sdkCreateTimer(&kernelTime);
    sdkResetTimer(&kernelTime);
    
    Md = AllocateDeviceMatrix(M.width, M.height);
    Nd = AllocateDeviceMatrix(N.width, N.height);
    Pd = AllocateDeviceMatrix(P.width, P.height);

    //matrice kernel
    size = M.width * M.height * sizeof(float); 
    cudaMemcpy(Md.elements, M.elements, size, cudaMemcpyHostToDevice);
    
    //matrice inițială de pe host
    size = N.width * N.height * sizeof(float); 
    cudaMemcpy( Nd.elements, N.elements, size, cudaMemcpyHostToDevice);
    
    dim3 dimBlock(BLOCK_SIZE, BLOCK_SIZE);
    dim3 dimGrid((N.width + dimBlock.x - 1) / dimBlock.x, (N.height + dimBlock.y - 1) / dimBlock.y);
    
    sdkStartTimer(&kernelTime);
    
    ConvolutionKernelShared<<<dimGrid, dimBlock>>>(Md, Nd, Pd);
        
    cudaThreadSynchronize();
    sdkStopTimer(&kernelTime);
    printf ("Timp execuție kernel cu memorie partajată: %f ms\n", sdkGetTimerValue(&kernelTime));
    
    size = P.width * P.height * sizeof(float); 
    cudaMemcpy(P.elements, Pd.elements, size, cudaMemcpyDeviceToHost);
    
    FreeDeviceMatrix(&Md);
    FreeDeviceMatrix(&Nd);
    FreeDeviceMatrix(&Pd);
}


// Alocă o matrice de dimensiune height*width pe device
Matrix AllocateDeviceMatrix(int width, int height)
{
    Matrix m;

    m.width = width;
    m.height = height;
    m.pitch = width;

    size_t size =  m.width * m.height * sizeof(float);
    cudaMalloc( (void**) &(m.elements), size);

    return m;
}

// Alocă matrice pe host de dimensiune height*width
Matrix AllocateMatrix(int width, int height)
{
    Matrix M;
    M.width = M.pitch = width;
    M.height = height;
    int size = M.width * M.height;    
    M.elements = (float*) malloc(size*sizeof(float));
    return M;
}    

// Eliberează o matrice de pe device
void FreeDeviceMatrix(Matrix* M)
{
    cudaFree(M->elements);
    M->elements = NULL;
}

// Eliberează o matrice de pe host
void FreeMatrix(Matrix* M)
{
    free(M->elements);
    M->elements = NULL;
}
