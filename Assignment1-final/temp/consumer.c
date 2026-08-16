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

    inet_pton(AF_INET,
              "127.0.0.1",
              &server_addr.sin_addr);

    if (connect(sock,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        perror("connect");
        close(sock);
        exit(1);
    }

    char type = 'C';
    write_full(sock, &type, 1);

    char response;

    if (read_full(sock, &response, 1) < 0)
    {
        close(sock);
        exit(1);
    }

    if (response == 0)
    {
        printf("Consumer: queue is EMPTY.\n");
        close(sock);
        return 1;
    }

    /*
     * No length is sent.
     * Read until '\0'.
     */
    char message[MAX_MSG_SIZE];
    size_t len = 0;

    while (len < MAX_MSG_SIZE)
    {
        char c;

        if (read_full(sock, &c, 1) < 0)
        {
            close(sock);
            exit(1);
        }

        message[len++] = c;

        if (c == '\0')
            break;
    }

    printf("Consumer: received message -> \"%s\"\n",
           message);

    close(sock);
    return 0;
}