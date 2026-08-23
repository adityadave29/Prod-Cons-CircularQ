#include "common.h"
#include <sys/uio.h>

int main(void)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0)
    {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect");
        close(sock);
        exit(1);
    }

    printf("Connected. Type a message and press Enter to send it.\n");
    printf("Type 'quit' to disconnect.\n\n");

    char line[MAX_MSG_SIZE + 1];

    while (1)
    {
        printf("> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL)
        {
            break;
        }

        /* strip trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
        {
            line[len - 1] = '\0';
            len--;
        }

        if (strcmp(line, "quit") == 0)
            break;

        if (len == 0 || len > MAX_MSG_SIZE)
        {
            printf("Invalid message size, try again.\n");
            continue;
        }

        /* [P][4-byte length][message] — single writev() per message */
        char type = 'P';
        uint32_t net_len = htonl((uint32_t)len);

        struct iovec iov[3];
        iov[0].iov_base = &type;
        iov[0].iov_len = sizeof(type);
        iov[1].iov_base = &net_len;
        iov[1].iov_len = sizeof(net_len);
        iov[2].iov_base = line;
        iov[2].iov_len = len;

        size_t total_len = iov[0].iov_len + iov[1].iov_len + iov[2].iov_len;
        ssize_t written = writev(sock, iov, 3);

        if (written < 0 || (size_t)written != total_len)
        {
            perror("writev");
            break;
        }

        char response;
        if (read_full(sock, &response, 1) < 0)
        {
            fprintf(stderr, "Server closed connection unexpectedly.\n");
            break;
        }

        if (response == 1)
            printf("Producer: message \"%s\" added successfully.\n", line);
        else
            printf("Producer: queue is FULL, message rejected.\n");
    }

    /* Tell the server we're done, then close */
    char quit_header[5] = {'Q', 0, 0, 0, 0};
    write_full(sock, quit_header, sizeof(quit_header));

    close(sock);
    printf("Disconnected.\n");
    return 0;
}