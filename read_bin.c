/*=============================================================================

gcc -O3 -mavx2 -pthread read_bin.c -o read_bin

./read_bin

Currently very buggy (as expected from LLM lol) and only 24 GB/s (4 x PCIE5.0x4 NVMEs)

Huge room for improvements

=============================================================================*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <immintrin.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>

#define ALIGNMENT 4096
#define BUFFER_SIZE (64 * 1024 * 1024)
#define FILE_SIZE (10ULL * 1024 * 1024 * 1024)

// Helper function to sum up 32-bit integers in a vector
static inline uint32_t sum_avx2(__m256i v) {
    __m128i sum128 = _mm_add_epi32(
        _mm256_extracti128_si256(v, 0),
        _mm256_extracti128_si256(v, 1)
    );
    sum128 = _mm_add_epi32(sum128, _mm_srli_si128(sum128, 8));
    sum128 = _mm_add_epi32(sum128, _mm_srli_si128(sum128, 4));
    return _mm_cvtsi128_si32(sum128);
}

// Structure to pass data to each thread.
typedef struct {
    const char *filename;
    uint64_t sum;        // computed checksum from the file
    size_t total_bytes;  // total bytes read
} thread_data_t;

void *read_file(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    int fd = open(data->filename, O_RDONLY | O_DIRECT);
    if (fd < 0) {
        fprintf(stderr, "Error opening %s: %s\n", data->filename, strerror(errno));
        pthread_exit((void *)1);
    }

    // Get file size (for informational purposes)
    off_t file_size = lseek(fd, 0, SEEK_END);
    if (file_size < 0) {
        perror("lseek");
        close(fd);
        pthread_exit((void *)1);
    }
    lseek(fd, 0, SEEK_SET);

    void *buf;
    if (posix_memalign(&buf, ALIGNMENT, BUFFER_SIZE)) {
        perror("posix_memalign");
        close(fd);
        pthread_exit((void *)1);
    }
    
    uint64_t local_sum = 0;
    size_t total = 0;
    
    while (1) {
        ssize_t nread = read(fd, buf, BUFFER_SIZE);
        if (nread < 0) {
            perror("read");
            break;
        }
        if (nread == 0)
            break;

        uint8_t *p = (uint8_t *)buf;
        size_t vec_size = nread / 32; // Process 32 bytes at a time
        size_t remainder = nread % 32;

        // Initialize accumulator vectors
        __m256i sum_vec1 = _mm256_setzero_si256();
        __m256i sum_vec2 = _mm256_setzero_si256();
        
        // Process 32 bytes at a time using AVX2
        for (size_t i = 0; i < vec_size; i++) {
            __m256i data_vec = _mm256_loadu_si256((__m256i*)(p + i * 32));
            
            // Unpack bytes to 16-bit integers
            __m256i zero = _mm256_setzero_si256();
            __m256i low = _mm256_unpacklo_epi8(data_vec, zero);
            __m256i high = _mm256_unpackhi_epi8(data_vec, zero);
            
            // Convert to 32-bit integers and accumulate
            sum_vec1 = _mm256_add_epi32(sum_vec1, _mm256_unpacklo_epi16(low, zero));
            sum_vec1 = _mm256_add_epi32(sum_vec1, _mm256_unpackhi_epi16(low, zero));
            sum_vec2 = _mm256_add_epi32(sum_vec2, _mm256_unpacklo_epi16(high, zero));
            sum_vec2 = _mm256_add_epi32(sum_vec2, _mm256_unpackhi_epi16(high, zero));
        }
        
        // Combine the two accumulator vectors
        __m256i sum_vec = _mm256_add_epi32(sum_vec1, sum_vec2);
        uint32_t vector_sum = sum_avx2(sum_vec);
        
        // Process remaining bytes
        for (size_t i = nread - remainder; i < nread; i++) {
            vector_sum += p[i];
        }
        
        local_sum += vector_sum;
        total += nread;
    }

    free(buf);
    close(fd);
    data->sum = local_sum;
    data->total_bytes = total;
    
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    // Hard-code the four file paths.
    const char *files[4] = {
        "/mnt/nvme1/10GB.bin",
        "/mnt/nvme1/10GB.bin",
        "/mnt/nvme2/10GB.bin",
        "/mnt/nvme3/10GB.bin"
    };

    const int num_threads = 4;
    pthread_t threads[num_threads];
    thread_data_t thread_data[num_threads];

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Create threads to process each file concurrently.
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].filename = files[i];
        thread_data[i].sum = 0;
        thread_data[i].total_bytes = 0;
        int ret = pthread_create(&threads[i], NULL, read_file, &thread_data[i]);
        if (ret != 0) {
            fprintf(stderr, "Error creating thread for file %s: %s\n", files[i], strerror(ret));
            // If thread creation fails, terminate.
            exit(EXIT_FAILURE);
        }
    }

    // Wait for all threads to finish.
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    // Combine results from all threads.
    uint64_t total_sum = 0;
    size_t grand_total_bytes = 0;
    for (int i = 0; i < num_threads; i++) {
        total_sum += thread_data[i].sum;
        grand_total_bytes += thread_data[i].total_bytes;
    }

    double elapsed = (end.tv_sec - start.tv_sec) + 
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    double speed = (grand_total_bytes / (1024.0 * 1024.0 * 1024.0)) / elapsed;

    printf("Overall Checksum: %lu\n", total_sum);
    printf("Total bytes read: %zu\n", grand_total_bytes);
    printf("Elapsed time: %.2f seconds\n", elapsed);
    printf("Processing speed: %.2f GB/s\n", speed);

    return 0;
}
