#include "common.h"
#include <pthread.h>
#include <signal.h>

typedef struct
{
    char *buffer;
    size_t capacity;
    size_t used;
    size_t head;
    size_t tail;
    pthread_mutex_t lock;
} CircularQueue;

CircularQueue q;
int server_fd;

/* Initialize queue */
void queue_init(size_t capacity)
{
    q.buffer = malloc(capacity);
    if (q.buffer == NULL)
    {
        perror("malloc");
        exit(1);
    }

    q.capacity = capacity;
    q.used = 0;
    q.head = 0;
    q.tail = 0;

    pthread_mutex_init(&q.lock, NULL);
}

/* Write data into circular buffer */
void circular_write(size_t *position, const void *data, size_t len)
{
    const char *src = (const char *)data;
    size_t first_part = q.capacity - *position;

    if (first_part >= len)
    {
        memcpy(q.buffer + *position, src, len);
        *position = (*position + len) % q.capacity;
    }
    else
    {
        memcpy(q.buffer + *position, src, first_part);
        memcpy(q.buffer, src + first_part, len - first_part);
        *position = len - first_part;
    }
}

/* Read data from circular buffer */
void circular_read(size_t *position, void *data, size_t len)
{
    char *dest = (char *)data;
    size_t first_part = q.capacity - *position;

    if (first_part >= len)
    {
        memcpy(dest, q.buffer + *position, len);
        *position = (*position + len) % q.capacity;
    }
    else
    {
        memcpy(dest, q.buffer + *position, first_part);
        memcpy(dest + first_part, q.buffer, len - first_part);
        *position = len - first_part;
    }
}

/* Add message to queue */
int queue_push(const char *data, size_t len)
{
    pthread_mutex_lock(&q.lock);

    size_t required = sizeof(uint32_t) + len;

    if (q.used + required > q.capacity)
    {
        pthread_mutex_unlock(&q.lock);
        return 0;
    }

    uint32_t net_len = htonl((uint32_t)len);

    circular_write(&q.tail, &net_len, sizeof(net_len));
    circular_write(&q.tail, data, len);

    q.used += required;

    pthread_mutex_unlock(&q.lock);
    return 1;
}

/* Remove oldest message */
char *queue_pop(size_t *len)
{
    pthread_mutex_lock(&q.lock);

    if (q.used == 0)
    {
        pthread_mutex_unlock(&q.lock);
        return NULL;
    }

    uint32_t net_len;
    circular_read(&q.head, &net_len, sizeof(net_len));

    size_t message_len = ntohl(net_len);

    if (message_len == 0 ||
        message_len > MAX_MSG_SIZE ||
        sizeof(uint32_t) + message_len > q.used)
    {
        pthread_mutex_unlock(&q.lock);
        return NULL;
    }

    char *data = malloc(message_len);
    if (data == NULL)
    {
        pthread_mutex_unlock(&q.lock);
        return NULL;
    }

    circular_read(&q.head, data, message_len);

    q.used -= sizeof(uint32_t) + message_len;
    *len = message_len;

    pthread_mutex_unlock(&q.lock);
    return data;
}

/* Free queue memory */
void queue_destroy(void)
{
    pthread_mutex_lock(&q.lock);

    free(q.buffer);
    q.buffer = NULL;
    q.used = 0;
    q.head = 0;
    q.tail = 0;

    pthread_mutex_unlock(&q.lock);
    pthread_mutex_destroy(&q.lock);
}

/* Server shutdown */
void handle_shutdown(int sig)
{
    (void)sig;

    printf("\nShutting down server...\n");
    queue_destroy();
    close(server_fd);
    exit(0);
}

/* Send a full HTTP response: status line + Content-Length + body */
void send_response(int fd, int status, const char *status_text,
                   const char *body, size_t body_len)
{
    char header[256];
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 %d %s\r\n"
                              "Content-Length: %zu\r\n"
                              "Connection: close\r\n"
                              "\r\n",
                              status, status_text, body_len);

    if (write_full(fd, header, (size_t)header_len) < 0)
        return;

    if (body != NULL && body_len > 0)
        write_full(fd, body, body_len);
}

/* POST /produce  -> body is the raw message, length given by Content-Length */
void handle_produce(int client_fd, long content_length)
{
    if (content_length <= 0 || content_length > MAX_MSG_SIZE)
    {
        send_response(client_fd, 400, "Bad Request", "INVALID_LENGTH", 14);
        return;
    }

    char *message = malloc((size_t)content_length);
    if (message == NULL)
    {
        send_response(client_fd, 500, "Internal Server Error", "OOM", 3);
        return;
    }

    if (read_full(client_fd, message, (size_t)content_length) < 0)
    {
        free(message);
        return; /* connection dropped, nothing to respond to */
    }

    int success = queue_push(message, (size_t)content_length);
    free(message);

    if (success)
    {
        printf("[PRODUCER] Message added (used=%zu/%zu bytes)\n",
               q.used, q.capacity);
        send_response(client_fd, 200, "OK", "ACCEPTED", 8);
    }
    else
    {
        printf("[PRODUCER] Queue FULL, producer rejected\n");
        send_response(client_fd, 507, "Insufficient Storage", "QUEUE_FULL", 10);
    }
}

/* GET /consume -> pops oldest message and returns it as the response body */
void handle_consume(int client_fd)
{
    size_t len;
    char *message = queue_pop(&len);

    if (message == NULL)
    {
        printf("[CONSUMER] Queue EMPTY\n");
        send_response(client_fd, 204, "No Content", NULL, 0);
        return;
    }

    printf("[CONSUMER] Message removed (used=%zu/%zu bytes)\n",
           q.used, q.capacity);
    send_response(client_fd, 200, "OK", message, len);
    free(message);
}

/* Handle one client connection: parse request line + headers, route it */
void *handle_client(void *arg)
{
    int client_fd = *(int *)arg;
    free(arg);

    char line[MAX_HEADER_LINE];

    /* Request line: "METHOD PATH HTTP/1.1" */
    if (read_line(client_fd, line, sizeof(line)) < 0)
    {
        close(client_fd);
        return NULL;
    }

    char method[8] = {0};
    char path[256] = {0};
    sscanf(line, "%7s %255s", method, path);

    /* Headers, until the blank line. We only care about Content-Length. */
    long content_length = 0;

    while (1)
    {
        int n = read_line(client_fd, line, sizeof(line));

        if (n < 0)
        {
            close(client_fd);
            return NULL;
        }

        if (n == 0)
            break; /* blank line -> end of headers */

        if (strncasecmp(line, "Content-Length:", 15) == 0)
            content_length = atol(line + 15);
    }

    if (strcmp(method, "POST") == 0 && strcmp(path, "/produce") == 0)
    {
        handle_produce(client_fd, content_length);
    }
    else if (strcmp(method, "GET") == 0 && strcmp(path, "/consume") == 0)
    {
        /* Any body on a GET is ignored/unexpected; nothing to drain here
           since our clients never send one. */
        handle_consume(client_fd);
    }
    else
    {
        send_response(client_fd, 404, "Not Found", "NOT_FOUND", 9);
    }

    close(client_fd);
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <queue_size_in_KB>\n", argv[0]);
        exit(1);
    }

    long kb = atol(argv[1]);

    if (kb <= 0)
    {
        fprintf(stderr, "Invalid queue size\n");
        exit(1);
    }

    size_t capacity = (size_t)kb * 1024;
    queue_init(capacity);

    signal(SIGINT, handle_shutdown);
    signal(SIGTERM, handle_shutdown);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0)
    {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        queue_destroy();
        exit(1);
    }

    if (listen(server_fd, 50) < 0)
    {
        perror("listen");
        queue_destroy();
        exit(1);
    }

    printf("Server started\n");
    printf("Queue capacity = %ld KB\n", kb);
    printf("Listening on port %d\n", PORT);
    printf("Endpoints: POST /produce, GET /consume\n");

    while (1)
    {
        int *client_fd = malloc(sizeof(int));

        if (client_fd == NULL)
            continue;

        *client_fd = accept(server_fd, NULL, NULL);

        if (*client_fd < 0)
        {
            free(client_fd);
            continue;
        }

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, client_fd);
        pthread_detach(tid);
    }

    return 0;
}