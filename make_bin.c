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

#define FILE_SIZE (10LL * 1024LL * 1024LL * 1024LL)  // 10GB
#define BUFFER_SIZE (1024 * 1024)

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }

    assert(open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644) != -1);

    // sum of bytes for all 4 files = 4*10*1024*(1024*1024*2-1) = 85899304960
    unsigned char *buffer = malloc(BUFFER_SIZE);
    for (int i = 0; i < BUFFER_SIZE; i++) {
        buffer[i] = (i % 3) + 1;
    }

    ssize_t bytes_written;
    long long remaining_bytes = FILE_SIZE;

    while (remaining_bytes > 0) {
        size_t write_size = (remaining_bytes >= BUFFER_SIZE) ? BUFFER_SIZE : remaining_bytes;
        bytes_written = write(fd, buffer, write_size);
        assert (bytes_written != -1);
        remaining_bytes -= bytes_written;
    }

    free(buffer);
    close(fd);
    printf("Successfully created 10GB file at %s\n", argv[1]);
    return 0;
}