#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

int main(int argc, char *argv[])
{
    long long sz;

    char buf[1];
    char write_buf[] = "testing writing";
    int offset = 100; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (argc == 2) {
        for (int i = 0; i <= offset; i++) {
            for (int j = 0; j < 100; j++) {
                struct timespec t1, t2;
                lseek(fd, i, SEEK_SET);
                clock_gettime(CLOCK_MONOTONIC, &t1);
                sz = write(fd, write_buf, atoi(argv[1]));
                clock_gettime(CLOCK_MONOTONIC, &t2);
                printf("%d %lld %ld %lld\n", i, sz, t2.tv_nsec - t1.tv_nsec,
                       t2.tv_nsec - t1.tv_nsec - sz);
            }
        }
        return 0;
    }
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               i, sz);
    }

    for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               i, sz);
    }

    close(fd);
    return 0;
}
