#include "common.h"
#include <pthread.h>
#include <signal.h>
#include <sys/uio.h>

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

void *handle_client(void *arg)
{
    int client_fd = *(int *)arg;
    free(arg);

    while (1)
    {
        /*
         * Fixed 5-byte request header for every request:
         *   byte 0     -> type ('P', 'C', or 'Q')
         *   bytes 1..4 -> length (meaningful only for 'P')
         */
        char header[5];

        if (read_full(client_fd, header, sizeof(header)) < 0)
            break;
        char type = header[0];

        /* Explicit disconnect request */
        if (type == 'Q')
        {
            printf("[CLIENT] Requested disconnect\n");
            break;
        }

        /* Producer */
        if (type == 'P')
        {
            uint32_t net_len;
            memcpy(&net_len, header + 1, sizeof(net_len));

            size_t len = ntohl(net_len);

            if (len == 0 || len > MAX_MSG_SIZE)
                break;

            char *message = malloc(len);
            if (message == NULL)
                break;

            /* 2nd read: size only known after decoding the header */
            if (read_full(client_fd, message, len) < 0)
            {
                free(message);
                break;
            }

            int success = queue_push(message, len);
            free(message);

            char response = success ? 1 : 0;
            write_full(client_fd, &response, 1);

            if (success)
                printf("[PRODUCER] Message added (used=%zu/%zu bytes)\n", q.used, q.capacity);
            else
                printf("[PRODUCER] Queue FULL, producer rejected\n");
        }

        /* Consumer */
        else if (type == 'C')
        {
            size_t len = 0;
            char *message = queue_pop(&len);

            char resp_header[5];

            if (message == NULL)
            {
                resp_header[0] = 0;
                memset(resp_header + 1, 0, sizeof(uint32_t));

                write_full(client_fd, resp_header, sizeof(resp_header));
                printf("[CONSUMER] Queue EMPTY\n");
            }
            else
            {
                uint32_t net_len = htonl((uint32_t)len);

                resp_header[0] = 1;
                memcpy(resp_header + 1, &net_len, sizeof(net_len));

                struct iovec iov[2];
                iov[0].iov_base = resp_header;
                iov[0].iov_len = sizeof(resp_header);
                iov[1].iov_base = message;
                iov[1].iov_len = len;

                size_t total_len = iov[0].iov_len + iov[1].iov_len;
                ssize_t written = writev(client_fd, iov, 2);

                if (written < 0 || (size_t)written != total_len)
                    perror("writev");
                else
                    printf("[CONSUMER] Message removed (used=%zu/%zu bytes)\n",
                           q.used, q.capacity);

                free(message);
            }
        }
        else
        {
            break;
        }
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
