#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 9090
#define MAX_MSG_SIZE 65536

static inline int read_full(int fd, void *buf, size_t n)
{
    size_t total = 0;
    char *p = (char *)buf;

    while (total < n) {
        ssize_t r = read(fd, p + total, n - total);
        if (r <= 0)
            return -1;

        total += r;
    }

    return 0;
}

static inline int write_full(int fd, const void *buf, size_t n)
{
    size_t total = 0;
    const char *p = (const char *)buf;

    while (total < n) {
        ssize_t w = write(fd, p + total, n - total);
        if (w <= 0)
            return -1;

        total += w;
    }

    return 0;
}

#endif