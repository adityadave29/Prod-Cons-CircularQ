#include "common.h"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s \"message text\"\n", argv[0]);
        exit(1);
    }

    const char *message = argv[1];
    size_t len = strlen(message);

    if (len == 0 || len > MAX_MSG_SIZE)
    {
        fprintf(stderr, "Invalid message size\n");
        exit(1);
    }

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
     * POST /produce HTTP/1.1
     * Content-Length: <len>
     *
     * <message bytes>
     */

    char header[256];
    int header_len = snprintf(header, sizeof(header),
                              "POST /produce HTTP/1.1\r\n"
                              "Content-Length: %zu\r\n"
                              "\r\n",
                              len);

    write_full(sock, header, (size_t)header_len);
    write_full(sock, message, len);

    /* Read response status line, e.g. "HTTP/1.1 200 OK" */
    char line[MAX_HEADER_LINE];

    if (read_line(sock, line, sizeof(line)) < 0)
    {
        close(sock);
        exit(1);
    }

    int status = 0;
    sscanf(line, "HTTP/1.1 %d", &status);

    /* Read headers to find Content-Length, then drain the body */
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

    if (content_length > 0)
    {
        char *body = malloc((size_t)content_length);
        if (body != NULL)
        {
            read_full(sock, body, (size_t)content_length);
            free(body);
        }
    }

    close(sock);

    if (status == 200)
    {
        printf("Producer: message \"%s\" added successfully.\n", message);
        return 0;
    }

    printf("Producer: queue is FULL. Terminating producer.\n");
    return 1;
}