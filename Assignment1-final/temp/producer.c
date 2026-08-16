#include "common.h"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr,
                "Usage: %s \"message\"\n",
                argv[0]);
        exit(1);
    }

    const char *message = argv[1];

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

    char type = 'P';
    write_full(sock, &type, 1);

    /*
     * Send normal C string including '\0'.
     */
    write_full(sock,
               message,
               strlen(message) + 1);

    char response;

    if (read_full(sock, &response, 1) < 0)
    {
        close(sock);
        exit(1);
    }

    close(sock);

    if (response == 1)
    {
        printf("Producer: message \"%s\" added successfully.\n",
               message);
        return 0;
    }

    printf("Producer: queue is FULL.\n");
    return 1;
}