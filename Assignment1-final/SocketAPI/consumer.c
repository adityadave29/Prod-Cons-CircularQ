#include "common.h"

int main(void)
{
    /* Create socket */
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

    /* Connect to server */
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect");
        close(sock);
        exit(1);
    }

    /*
     * Protocol: plain HTTP/1.1
     *
     * GET /consume HTTP/1.1
     *
     * Response:
     *   204 No Content  -> queue empty
     *   200 OK          -> body is the message, length in Content-Length
     */

    const char *request = "GET /consume HTTP/1.1\r\n\r\n";
    write_full(sock, request, strlen(request));

    /* Read response status line */
    char line[MAX_HEADER_LINE];

    if (read_line(sock, line, sizeof(line)) < 0)
    {
        close(sock);
        exit(1);
    }

    int status = 0;
    sscanf(line, "HTTP/1.1 %d", &status);

    /* Read headers to find Content-Length */
    long content_length = 0;

    while (1)
    {
        int n = read_line(sock, line, sizeof(line));

        if (n < 0)
        {
            close(sock);
            exit(1);
        }

        if (n == 0)
            break;

        if (strncasecmp(line, "Content-Length:", 15) == 0)
            content_length = atol(line + 15);
    }

    if (status == 204 || content_length <= 0)
    {
        printf("Consumer: queue is EMPTY. Terminating consumer.\n");
        close(sock);
        return 1;
    }

    if (content_length > MAX_MSG_SIZE)
    {
        fprintf(stderr, "Consumer: response too large\n");
        close(sock);
        exit(1);
    }

    char *message = malloc((size_t)content_length + 1);

    if (message == NULL)
    {
        close(sock);
        exit(1);
    }

    if (read_full(sock, message, (size_t)content_length) < 0)
    {
        free(message);
        close(sock);
        exit(1);
    }

    message[content_length] = '\0';
    printf("Consumer: received message -> \"%s\"\n", message);
    free(message);
    close(sock);
    return 0;
}