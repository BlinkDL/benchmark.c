/*=============================================================================

gcc -O3 -march=native -o nvme nvme.c -lrt

sudo ./nvme

check speed in iotop

53+ GB/s (4 x PCIE5.0x4 NVMEs)

=============================================================================*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#define BLOCK_SIZE (1UL << 29)    // 512MB per block
#define COUNT 500                  // number of blocks to read
#define ALIGNMENT 4096            // alignment required by O_DIRECT
#define NUM_DEVICES 4

typedef struct {
    const char *device;   // device path (e.g., "/dev/nvme0n1")
    double throughput;    // in GB/s
    double elapsed;       // seconds taken to read COUNT blocks
    int error;            // flag for any error encountered
} thread_data_t;

static void *bench_thread(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    int fd = open(data->device, O_RDONLY | O_DIRECT);
    if (fd < 0) {
        fprintf(stderr, "Error opening %s: %s\n", data->device, strerror(errno));
        data->error = 1;
        pthread_exit(NULL);
    }

    // Allocate an aligned buffer for I/O.
    void *buffer = NULL;
    if (posix_memalign(&buffer, ALIGNMENT, BLOCK_SIZE) != 0) {
        fprintf(stderr, "Error in posix_memalign for %s: %s\n", data->device, strerror(errno));
        close(fd);
        data->error = 1;
        pthread_exit(NULL);
    }

    // Get start timestamp.
    struct timespec start, end;
    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        fprintf(stderr, "Error in clock_gettime for %s: %s\n", data->device, strerror(errno));
        free(buffer);
        close(fd);
        data->error = 1;
        pthread_exit(NULL);
    }

    // Perform COUNT reads of BLOCK_SIZE bytes each.
    for (int i = 0; i < COUNT; i++) {
        ssize_t bytesRead = read(fd, buffer, BLOCK_SIZE);
        if (bytesRead < 0) {
            fprintf(stderr, "Error reading from %s: %s\n", data->device, strerror(errno));
            free(buffer);
            close(fd);
            data->error = 1;
            pthread_exit(NULL);
        } else if (bytesRead == 0) {
            // End-of-file/device reached earlier.
            break;
        }
    }

    // Get end timestamp.
    if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
        fprintf(stderr, "Error in clock_gettime for %s: %s\n", data->device, strerror(errno));
        free(buffer);
        close(fd);
        data->error = 1;
        pthread_exit(NULL);
    }

    free(buffer);
    close(fd);

    // Calculate elapsed time.
    data->elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    // Calculate throughput. Total bytes read = COUNT * BLOCK_SIZE.
    data->throughput = ((double)COUNT * BLOCK_SIZE) / data->elapsed;
    // Convert throughput to GB/s.
    data->throughput /= 1e9;

    pthread_exit(NULL);
}

int main(void) {
    // List the device names.
    const char *devices[NUM_DEVICES] = {
        "/dev/nvme0n1",
        "/dev/nvme1n1",
        "/dev/nvme2n1",
        "/dev/nvme3n1"
    };

    pthread_t threads[NUM_DEVICES];
    thread_data_t thread_data[NUM_DEVICES] = {0};

    // Create a thread for each device.
    for (int i = 0; i < NUM_DEVICES; i++) {
        thread_data[i].device = devices[i];
        thread_data[i].throughput = 0.0;
        thread_data[i].elapsed = 0.0;
        thread_data[i].error = 0;

        int ret = pthread_create(&threads[i], NULL, bench_thread, &thread_data[i]);
        if (ret != 0) {
            fprintf(stderr, "Error creating thread for %s: %s\n", devices[i], strerror(ret));
            // In a real program, you might try to cancel already spawned threads.
            return EXIT_FAILURE;
        }
    }

    // Wait for all threads to complete.
    double combined_throughput = 0.0;
    for (int i = 0; i < NUM_DEVICES; i++) {
        pthread_join(threads[i], NULL);
        if (thread_data[i].error) {
            fprintf(stderr, "Benchmark failed for %s\n", thread_data[i].device);
        } else {
            printf("Device %s:\n", thread_data[i].device);
            printf("  Elapsed time: %.2f seconds\n", thread_data[i].elapsed);
            printf("  Throughput:  %.2f GB/s\n", thread_data[i].throughput);
            combined_throughput += thread_data[i].throughput;
        }
    }
    printf("\nCombined throughput: %.2f GB/s\n", combined_throughput);
    return EXIT_SUCCESS;
}
