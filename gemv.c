/*

gcc-14 -O3 -mavx2 -mavx512f -march=native -o gemv gemv.c && ./gemv

i get 75 ~ 147 GB/s (?!)

*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <immintrin.h>
#include <time.h>
#include <string.h>

double get_time_in_seconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

void gemv_int8_avx512(const int8_t *A, const int8_t *x, int32_t *y, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        __m512i acc = _mm512_setzero_si512(); // Initialize accumulator to zero
        for (int j = 0; j < cols; j += 64) {
            __m512i a = _mm512_loadu_si512((__m512i*)&A[i * cols + j]); // Load 64 int8 elements from the matrix row            
            __m512i b = _mm512_loadu_si512((__m512i*)&x[j]); // Load 64 int8 elements from the vector
            // Multiply and accumulate (int8 -> int16 -> int32)
            __m512i prod = _mm512_maddubs_epi16(a, b); // Multiply unsigned bytes and add pairs
            __m512i prod32 = _mm512_madd_epi16(prod, _mm512_set1_epi16(1)); // Horizontal add to int32
            acc = _mm512_add_epi32(acc, prod32); // Accumulate
        }        
        y[i] = _mm512_reduce_add_epi32(acc); // Horizontally sum the 16 int32 values in the AVX-512 register
    }
}

void benchmark_gemv(int rows, int cols) {
    int8_t *A = (int8_t*)aligned_alloc(64, rows * cols * sizeof(int8_t));
    int8_t *x = (int8_t*)aligned_alloc(64, cols * sizeof(int8_t));
    int32_t *y = (int32_t*)aligned_alloc(64, rows * sizeof(int32_t));

    // Initialize matrix and vector with random values
    for (int i = 0; i < rows * cols; i++) A[i] = rand() % 256 - 128;
    for (int i = 0; i < cols; i++) x[i] = rand() % 256 - 128;
    
    gemv_int8_avx512(A, x, y, rows, cols); // Warm-up

    // Benchmark multiple times and track the fastest time
    double fastest_time = 1e9; // Initialize to a very large value
    for (int trial = 0; trial < 1000; trial++) {
        double start = get_time_in_seconds();
        gemv_int8_avx512(A, x, y, rows, cols);
        double end = get_time_in_seconds();
        double elapsed = end - start;
        if (elapsed < fastest_time) {
            fastest_time = elapsed;
        }
    }

    // Calculate performance
    double bytes_accessed = (rows * cols + cols + rows) * sizeof(int8_t); // A, x, and y
    double bandwidth = bytes_accessed / fastest_time / 1e9; // GB/s

    printf("GEMV %dx%d: Fastest Time = %.6f s, Bandwidth = %.2f GB/s\n", rows, cols, fastest_time, bandwidth);

    // Free memory
    free(A);
    free(x);
    free(y);
}

int main() {
    // Benchmark for various matrix dimensions
    for (int rows = 1024; rows <= 8192; rows *= 2) {
        for (int cols = 1024; cols <= 8192; cols *= 2) {
            benchmark_gemv(rows, cols);
        }
    }
    return 0;
}