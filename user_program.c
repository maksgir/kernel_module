#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#define DEBUGFS_PATH "/sys/kernel/debug/my_kernel_module/block_device_stats"

struct Message {
    char name[DISK_NAME_LEN];
    unsigned long rd;
    unsigned long rd_sectors;
    unsigned long wr;
    unsigned long wr_sectors;
};

int main() {
    int fd = open(DEBUGFS_PATH, O_RDWR);
    if (fd == -1) {
        perror("Error opening file in debugfs");
        return EXIT_FAILURE;
    }
    printf("File opened");
    struct Message buffer[10]; // Assuming MESSAGE_STORAGE_SIZE is 10

    ssize_t bytesRead = read(fd, buffer, sizeof(buffer));
    if (bytesRead == -1) {
        perror("Error reading from file in debugfs");
        close(fd);
        return EXIT_FAILURE;
    }

    int i;
    for (i = 0; i < bytesRead / sizeof(struct Message); ++i) {
        printf("Disk Name: %s\n", buffer[i].name);
        printf("Read: %lu IOs, %lu sectors\n", buffer[i].rd, buffer[i].rd_sectors);
        printf("Write: %lu IOs, %lu sectors\n", buffer[i].wr, buffer[i].wr_sectors);
        printf("\n");
    }

    close(fd);
    return EXIT_SUCCESS;
}

