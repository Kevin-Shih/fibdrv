#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"
#define BUF_SIZE 2048

int main(int argc, char *argv[])
{
    long long sz;

    char *buf = calloc(BUF_SIZE, 1);
    int offset = 500; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (argc == 2) {
        for (int i = 0; i <= offset; i++) {
            for (int j = 0; j < 100; j++) {
                struct timespec t1, t2;
                lseek(fd, i, SEEK_SET);
                clock_gettime(CLOCK_MONOTONIC, &t1);
                sz = write(fd, NULL, atoi(argv[1]));
                clock_gettime(CLOCK_MONOTONIC, &t2);
                long user_time;
                if ((t2.tv_nsec - t1.tv_nsec) < 0)
                    user_time = 1000000000 + t2.tv_nsec - t1.tv_nsec;
                else
                    user_time = t2.tv_nsec - t1.tv_nsec;
                printf("%d %lld %ld %lld\n", i, sz, user_time, user_time - sz);
            }
        }
        return 0;
    }
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, BUF_SIZE);
        printf("%3d %s\n", i, buf);
        memset(buf, 0, BUF_SIZE);
    }

    free(buf);
    close(fd);
    return 0;
}
