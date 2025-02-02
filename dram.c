/*=============================================================================

gcc-14 -O3 -march=native -mavx512f -o dram dram.c -lnuma && sudo ./dram

60.8 GB/s for DDR5-6000 dual channel & AMD Zen4 CPU

=============================================================================*/

#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <x86intrin.h>
#include <sched.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <numa.h>
#include <immintrin.h>
#include <fcntl.h>
#include <errno.h>

#define ARRAY_SIZE (4UL * 1024 * 1024 * 1024) // 4 GB
#define CACHE_LINE_SIZE 64

struct benchmark_config {
    bool use_prefetch;
};

static inline uint64_t get_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

double measure_read_bandwidth(uint8_t* array, size_t size, struct benchmark_config* config)
{    
    __m512i zero = _mm512_setzero_si512(); // clear 512-bit vector to use in _mm512_sad_epu8
    __m512i sum = _mm512_setzero_si512(); // use 512-bit vector of 8 x 64-bit counters
    
    uint64_t start_ns = get_ns();
    for (size_t i = 0; i < size; i += 64) {
        if (config->use_prefetch) {
            _mm_prefetch((char*)&array[i + 64], _MM_HINT_T0);
        }
        __m512i data = _mm512_load_si512((__m512i const*)&array[i]); // load 64 bytes (512 bits) from array        
        __m512i partial = _mm512_sad_epu8(data, zero); // compute 8 sums (one per group of 8 bytes) of the absolute differences between data and zero
        sum = _mm512_add_epi64(sum, partial);
    }
    double bandwidth = ((double)size) / ((double)(get_ns() - start_ns)); // here we use 1GB/s = 1e9 bytes/s
    
    uint64_t total_sum = _mm512_reduce_add_epi64(sum); // reduce the eight 64-bit sums to a single 64-bit result
    printf("[%.2f GB/s][checksum %lu]\n", bandwidth, total_sum);
    
    return bandwidth;
}

int main()
{
    assert(numa_available() != -1); // init NUMA

    // set real-time priority
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    assert(sched_setscheduler(0, SCHED_FIFO, &param) != -1);

    // bind to NUMA node and CPU core
    int numa_node = 0;
    int cpu_core = 0;    
    numa_run_on_node(numa_node);
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_core, &cpuset);
    assert(sched_setaffinity(0, sizeof(cpuset), &cpuset) != -1);
    printf("Bound to CPU core %d on NUMA node %d\n", cpu_core, numa_node);
    
    // allocate memory
    uint8_t* array = numa_alloc_onnode(ARRAY_SIZE, numa_node);
    assert(array);
    assert(((uintptr_t)array % 64) == 0); // ensure 64-byte alignment
    assert(mlock(array, ARRAY_SIZE) == 0); // lock memory to prevent swapping
    printf("Array size: %lu GB\n", ARRAY_SIZE / (1024 * 1024 * 1024));

    // init array
    for (size_t i = 0; i < ARRAY_SIZE; i++) array[i] = (uint8_t)(i);

    // benchmark
    double max_bandwidth = 0.0;
    for (int i = 0; i < 5; i++)
    {
        printf("[iter %d]", i+1);
        struct benchmark_config config = {true};
        double bandwidth = measure_read_bandwidth(array, ARRAY_SIZE, &config);        
        max_bandwidth = (bandwidth > max_bandwidth) ? bandwidth : max_bandwidth;
    }
    printf("\nMax: %.2f GB/s\n", max_bandwidth);

    // done
    numa_free(array, ARRAY_SIZE);
    return 0;
}