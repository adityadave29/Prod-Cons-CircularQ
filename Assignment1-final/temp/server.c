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

/* Add null-terminated message */
int queue_push(const char *data)
{
    size_t len = strlen(data) + 1; /* Include '\0' */

    pthread_mutex_lock(&q.lock);

    if (q.used + len > q.capacity)
    {
        pthread_mutex_unlock(&q.lock);
        return 0;
    }

    circular_write(&q.tail, data, len);
    q.used += len;

    pthread_mutex_unlock(&q.lock);
    return 1;
}

/* Remove message until '\0' */
char *queue_pop(void)
{
    pthread_mutex_lock(&q.lock);

    if (q.used == 0)
    {
        pthread_mutex_unlock(&q.lock);
        return NULL;
    }

    /*
     * Read one byte at a time until '\0'.
     * This is the important difference from the length-based version.
     */
    size_t max = q.used;
    char *data = malloc(max);

    if (data == NULL)
    {
        pthread_mutex_unlock(&q.lock);
        return NULL;
    }

    size_t count = 0;

    while (count < max)
    {
        char c;

        circular_read(&q.head, &c, 1);
        q.used--;

        data[count++] = c;

        if (c == '\0')
            break;
    }

    if (count == max && data[count - 1] != '\0')
    {
        free(data);
        pthread_mutex_unlock(&q.lock);
        return NULL;
    }

    pthread_mutex_unlock(&q.lock);
    return data;
}

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

    char type;

    if (read_full(client_fd, &type, 1) < 0)
    {
        close(client_fd);
        return NULL;
    }

    /* Producer */
    if (type == 'P')
    {
        char *message = malloc(MAX_MSG_SIZE);

        if (message == NULL)
        {
            close(client_fd);
            return NULL;
        }

        size_t len = 0;

        /*
         * Read until '\0'.
         * There is no message length.
         */
        while (len < MAX_MSG_SIZE)
        {
            char c;

            if (read_full(client_fd, &c, 1) < 0)
            {
                free(message);
                close(client_fd);
                return NULL;
            }

            message[len++] = c;

            if (c == '\0')
                break;
        }

        if (len == MAX_MSG_SIZE &&
            message[len - 1] != '\0')
        {
            free(message);
            close(client_fd);
            return NULL;
        }

        int success = queue_push(message);

        free(message);

        char response = success ? 1 : 0;
        write_full(client_fd, &response, 1);

        if (success)
            printf("[PRODUCER] Message added "
                   "(used=%zu/%zu bytes)\n",
                   q.used, q.capacity);
        else
            printf("[PRODUCER] Queue FULL\n");
    }

    /* Consumer */
    else if (type == 'C')
    {
        char *message = queue_pop();

        if (message == NULL)
        {
            char response = 0;
            write_full(client_fd, &response, 1);
            printf("[CONSUMER] Queue EMPTY\n");
        }
        else
        {
            char response = 1;
            write_full(client_fd, &response, 1);

            /* Send message including '\0' */
            write_full(client_fd,
                       message,
                       strlen(message) + 1);

            printf("[CONSUMER] Message removed "
                   "(used=%zu/%zu bytes)\n",
                   q.used, q.capacity);

            free(message);
        }
    }

    close(client_fd);
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr,
                "Usage: %s <queue_size_in_KB>\n",
                argv[0]);
        exit(1);
    }

    long kb = atol(argv[1]);

    if (kb <= 0)
    {
        fprintf(stderr, "Invalid queue size\n");
        exit(1);
    }

    queue_init((size_t)kb * 1024);

    signal(SIGINT, handle_shutdown);
    signal(SIGTERM, handle_shutdown);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0)
    {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd,
               SOL_SOCKET,
               SO_REUSEADDR,
               &opt,
               sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd,
             (struct sockaddr *)&addr,
             sizeof(addr)) < 0)
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
        pthread_create(&tid,
                       NULL,
                       handle_client,
                       client_fd);
        pthread_detach(tid);
    }

    return 0;
}