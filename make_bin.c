/*=============================================================================

gcc -O3 make_bin.c -o make_bin

./make_bin /mnt/nvme0/10GB.bin
./make_bin /mnt/nvme1/10GB.bin
./make_bin /mnt/nvme2/10GB.bin
./make_bin /mnt/nvme3/10GB.bin

ls -lh /mnt/nvme0/10GB.bin
ls -lh /mnt/nvme1/10GB.bin
ls -lh /mnt/nvme2/10GB.bin
ls -lh /mnt/nvme3/10GB.bin

head -c 30 /mnt/nvme0/10GB.bin | hexdump -C
head -c 30 /mnt/nvme1/10GB.bin | hexdump -C
head -c 30 /mnt/nvme2/10GB.bin | hexdump -C
head -c 30 /mnt/nvme3/10GB.bin | hexdump -C

tail -c 30 /mnt/nvme0/10GB.bin | hexdump -C
tail -c 30 /mnt/nvme1/10GB.bin | hexdump -C
tail -c 30 /mnt/nvme2/10GB.bin | hexdump -C
tail -c 30 /mnt/nvme3/10GB.bin | hexdump -C

=============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define ONE_GB (1024LL * 1024LL * 1024LL)
#define FILE_SIZE (10LL * ONE_GB)  // 10GB
#define BUFFER_SIZE (1024 * 1024)  // 1MB buffer

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("Error opening file");
        return 1;
    }

    // Create a 1MB buffer with repeating pattern of 1,2,3
    unsigned char *buffer = malloc(BUFFER_SIZE);
    if (buffer == NULL) {
        perror("Error allocating memory");
        close(fd);
        return 1;
    }

    for (int i = 0; i < BUFFER_SIZE; i++) {
        // buffer[i] = (i % 3) + 1;  // This will create pattern 1,2,3,1,2,3,... => read_bin.c will give WRONG checksum
        buffer[i] = 1;
    }

    // Calculate number of full buffers needed
    long long remaining_bytes = FILE_SIZE;
    ssize_t bytes_written;

    while (remaining_bytes > 0) {
        size_t write_size = (remaining_bytes >= BUFFER_SIZE) ? BUFFER_SIZE : remaining_bytes;
        bytes_written = write(fd, buffer, write_size);
        
        if (bytes_written == -1) {
            perror("Error writing to file");
            free(buffer);
            close(fd);
            return 1;
        }
        
        remaining_bytes -= bytes_written;
    }

    free(buffer);
    close(fd);
    printf("Successfully created 10GB file at %s\n", argv[1]);
    return 0;
}