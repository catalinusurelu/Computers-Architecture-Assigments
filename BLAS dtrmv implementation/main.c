// Usurelu Catalin Constantin
// 333CA

#include <stdio.h>
#include <cblas.h>
#include <stdlib.h>
#include <sys/time.h>
#include <math.h>
#include <string.h>

double double_rand()
{
    return drand48();
}

double* generate_matrix(size_t size, unsigned int seed)
{
    double *mat = calloc(size * size, sizeof(double));
    int i, j;

    if(mat == NULL)
    fprintf(stderr, "MALLOC");

    srand48(seed);

    for (i = 0; i < size; i++)
    {
        for(j = 0; j <= i; j++)
        {
            mat[i * size + j] = double_rand();
        }
    }

    return mat;
}

double* generate_vector(size_t size, unsigned int seed)
{
    double *vect = calloc(size, sizeof(double));
    int i;

    srand48(seed);

    for (i = 0; i < size; i++)
    {
        vect[i] = double_rand();
    }

    return vect;
}

void print_matrix(double* A, size_t size)
{
    size_t i, j;
    for(i = 0; i < size; i++)
    {
        for(j = 0; j < size; j++)
        {
            printf("%lf ", A[i * size + j]);
        }
        printf("\n");
    }

}

void print_vector(double* x, size_t size)
{
    int i;
    for(i = 0; i < size; i++)
        printf("%lf ", x[i]);
}

int compare_vectors(double*a, double*b, int size, double eps)
{
    int i;
    for(i = 0; i < size; i++)
        if(fabs(a[i] - b[i]) > eps)
        {
            printf("%lf != %lf \n", a[i], b[i]);
            return 0;
        }
    return 1;
}

void my_dtrmv_unoptimized(const enum CBLAS_ORDER Order, const enum CBLAS_UPLO Uplo,
                          const enum CBLAS_TRANSPOSE TransA, const enum CBLAS_DIAG Diag,
                          const int N, const double *A, const int lda,
                          double *X, const int incX)
{
    int i, j;
    double sum;

    for(i = N - 1; i >= 0; i--)
    {
        sum = 0.0;
        for(j = 0; j <= i; j++)
        {
            sum += X[j] * A[i * N + j];
        }
        X[i] = sum;
    }
}

void my_dtrmv_optimized(const enum CBLAS_ORDER Order, const enum CBLAS_UPLO Uplo,
                        const enum CBLAS_TRANSPOSE TransA, const enum CBLAS_DIAG Diag,
                        const int N, const double *A, const int lda,
                        double *X, const int incX)
{
    int i, j, repeat_j, left_j;
    register double sum1, sum2, sum3, sum4;
    register const double *pA1;
    register const double *pA2;
    register const double *pA3;
    register const double *pA4;
    register const double *pX;

    // Calculez cate 4 linii in paralele
    for(i = N - 1; i >= 3; i -= 4)
    {
        sum1 = 0.0;
        sum2 = 0.0;
        sum3 = 0.0;
        sum4 = 0.0;
        pA1 = A + (i - 3) * N;
        pA2 = A + (i - 2) * N;
        pA3 = A + (i - 1) * N;
        pA4 = A + (i - 0) * N;
        pX = X;

        // i -3 + 1 = intervalul
        // + 7 -> in loc sa facem ceil
        repeat_j = (i - 3 + 1 + 7) / 8;
        left_j = (i - 3 + 1 + 7) % 8;

        // Execut multiplii de 8 operatii
        //for(j = 0; j + 7 <= i - 3; j += 8)
        for(j = 0; repeat_j-- > 0;)
        {
            sum1 += *pX * *pA1 + pX[1] * pA1[1] + pX[2] * pA1[2] + pX[3] * pA1[3]
                 + pX[4] * pA1[4] + pX[5] * pA1[5] + pX[6] * pA1[6] + pX[7] * pA1[7];

            sum2 += *pX * *pA2+ pX[1] * pA2[1] + pX[2] * pA2[2] + pX[3] * pA2[3]
                 + pX[4] * pA2[4] + pX[5] * pA2[5] + pX[6] * pA2[6] + pX[7] * pA2[7];

            sum3 += *pX * *pA3 + pX[1] * pA3[1] + pX[2] * pA3[2] + pX[3] * pA3[3]
                 + pX[4] * pA3[4] + pX[5] * pA3[5] + pX[6] * pA3[6] + pX[7] * pA3[7];

            sum4 += *pX * *pA4 + pX[1] * pA4[1] + pX[2] * pA4[2] + pX[3] * pA4[3]
                 + pX[4] * pA4[4] + pX[5] * pA4[5] + pX[6] * pA4[6] + pX[7] * pA4[7];

            pA1 += 8;
            pA2 += 8;
            pA3 += 8;
            pA4 += 8;
            pX += 8;
        }

        // Execut restul operatiilor (ca nu avem exact multiplu de 8)
        for(j = 0; j < left_j ;j++)
        //for(j = j; j <= i - 3; j++)
        {
            sum1 += *pX * *pA1;
            sum2 += *pX * *pA2;
            sum3 += *pX * *pA3;
            sum4 += *pX * *pA4;

            pA1 += 1;
            pA2 += 1;
            pA3 += 1;
            pA4 += 1;
            pX += 1;
        }

        sum2 += *pX * *pA2;
        sum3 += *pX * *pA3 + pX[1] * pA3[1];
        sum4 += *pX * *pA4 + pX[1] * pA4[1] + pX[2] * pA4[2];

        X[i-3] = sum1;
        X[i-2] = sum2;
        X[i-1] = sum3;
        X[i] = sum4;
    }

    // "Varful" (matrice de 3 elemente) matricei nu corespunde
    // cu stilul de cod de mai sus, optimizat, asa ca il
    // fac in mod normal, deoarece nu prea mai avem ce optimiza
    for(i = N % 4 - 1; i >= 0; i--)
    {
        sum1 = 0.0;
        for(j = 0; j <= i; j++)
        {
            sum1 += X[j] * A[i * N + j];
        }
        X[i] = sum1;
    }
}

int main(int argc, char* argv[])
{

    int N;
    int matrix_seed = 0;
    int vector_seed = 1;
    double* A;
    double *x, *x_unoptimized, *x_optimized;
    int incX = 1;
    char* function_call;
    struct timeval start, end;
    float elapsed;

    function_call = argv[1];
    N = atoi(argv[2]);
    A = generate_matrix(N, matrix_seed);
    x = generate_vector(N, vector_seed);

    x_unoptimized = malloc(N * sizeof(double));
    memcpy(x_unoptimized, x, N * sizeof(double));

    x_optimized = malloc(N * sizeof(double));
    memcpy(x_optimized, x, N * sizeof(double));

    

    gettimeofday(&start, NULL);
    cblas_dtrmv(CblasRowMajor, CblasLower, CblasNoTrans, CblasNonUnit, N, A, N, x, incX);
    gettimeofday(&end, NULL);
    elapsed = ((end.tv_sec - start.tv_sec)*1000000.0f + end.tv_usec - start.tv_usec)/1000000.0f;

    if(strcmp(function_call, "blas") == 0)
    {
       
        printf("%d\t%lf\n", N, elapsed);
    }
    else if(strcmp(function_call, "neoptimizat") == 0)
    {
        gettimeofday(&start, NULL);
        my_dtrmv_unoptimized(CblasRowMajor, CblasLower, CblasNoTrans, CblasNonUnit, N, A, N, x_unoptimized, incX);
        gettimeofday(&end, NULL);
        elapsed = ((end.tv_sec - start.tv_sec)*1000000.0f + end.tv_usec - start.tv_usec)/1000000.0f;

        if(compare_vectors(x, x_unoptimized, N, 0.0001f) == 0)
        {
            fprintf(stderr, "Warning, results do not match!\n");
        }
        else
        {
            fprintf(stderr, "Congrats! results match!\n");
        }

        printf("%d\t%lf\n", N, elapsed);
    }
    else if(strcmp(function_call, "optimizat") == 0)
    {
        gettimeofday(&start, NULL);

        my_dtrmv_optimized(CblasRowMajor, CblasLower, CblasNoTrans, CblasNonUnit, N, A, N, x_optimized, incX);

        gettimeofday(&end, NULL);

        elapsed = ((end.tv_sec - start.tv_sec)*1000000.0f + end.tv_usec - start.tv_usec)/1000000.0f;
        printf("%d\t%lf\n", N, elapsed);

        if(compare_vectors(x, x_optimized, N, 0.0001f) == 0)
        {
            fprintf(stderr, "Warning, results do not match!\n");
        }
        else
        {
            fprintf(stderr, "Congrats! results match!\n");
        }
    }

    return 0;
}
