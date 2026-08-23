#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 9090
#define MAX_MSG_SIZE 65536
#define MAX_HEADER_LINE 1024

/* Read exactly n bytes from socket */
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

/* Write exactly n bytes to socket */
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

/*
 * Read a single line terminated by '\n' (a trailing '\r' is stripped).
 * Used to parse the HTTP request/status line and headers.
 * Returns the line length (>=0) on success, -1 on error/EOF,
 * -2 if the line didn't fit in max_len (header too long).
 */
static inline int read_line(int fd, char *buf, size_t max_len)
{
    size_t i = 0;

    while (1) {
        char c;
        ssize_t r = read(fd, &c, 1);

        if (r <= 0)
            return -1;

        if (c == '\n')
            break;

        if (c == '\r')
            continue;

        if (i >= max_len - 1)
            return -2;

        buf[i++] = c;
    }

    buf[i] = '\0';
    return (int)i;
}

#endif