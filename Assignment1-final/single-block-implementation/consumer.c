#include "common.h"

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

    printf("Connected. Press Enter to consume the next message.\n");
    printf("Type 'quit' to disconnect.\n\n");

    char line[16];

    while (1)
    {
        printf("> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL)
            break;

        size_t llen = strlen(line);
        if (llen > 0 && line[llen - 1] == '\n')
            line[llen - 1] = '\0';

        if (strcmp(line, "quit") == 0)
            break;

        char req_header[5];
        req_header[0] = 'C';
        memset(req_header + 1, 0, sizeof(uint32_t));

        if (write_full(sock, req_header, sizeof(req_header)) < 0)
        {
            fprintf(stderr, "Failed to send request.\n");
            break;
        }

        /* Response: fixed 5-byte header first */
        char resp_header[5];
        if (read_full(sock, resp_header, sizeof(resp_header)) < 0)
        {
            fprintf(stderr, "Server closed connection unexpectedly.\n");
            break;
        }

        if (resp_header[0] == 0)
        {
            printf("Consumer: queue is EMPTY.\n");
            continue;
        }

        uint32_t net_len;
        memcpy(&net_len, resp_header + 1, sizeof(net_len));
        size_t len = ntohl(net_len);

        if (len == 0 || len > MAX_MSG_SIZE)
        {
            fprintf(stderr, "Invalid message length from server.\n");
            break;
        }

        char *message = malloc(len + 1);
        if (message == NULL)
        {
            fprintf(stderr, "malloc failed.\n");
            break;
        }

        /* 2nd read: body, size only known after header was parsed */
        if (read_full(sock, message, len) < 0)
        {
            free(message);
            fprintf(stderr, "Failed to read message body.\n");
            break;
        }

        message[len] = '\0';
        printf("Consumer: received message -> \"%s\"\n", message);
        free(message);
    }

    char quit_header[5] = {'Q', 0, 0, 0, 0};
    write_full(sock, quit_header, sizeof(quit_header));

    close(sock);
    printf("Disconnected.\n");
    return 0;
}