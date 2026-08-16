#include "common.h"
#include <pthread.h>
#include <signal.h>

typedef struct Node {
    char *data;
    int len;
    struct Node *next;
} Node;

typedef struct {
    Node *head, *tail;
    long capacity;   /* bytes */
    long used;       /* bytes currently used */
    pthread_mutex_t lock;
} Queue;

Queue q;
int server_fd;

void queue_init(long capacity_bytes) {
    q.head = q.tail = NULL;
    q.capacity = capacity_bytes;
    q.used = 0;
    pthread_mutex_init(&q.lock, NULL);
}

/* returns 1 on success, 0 if queue is full */
int queue_push(const char *data, int len) {
    int ok = 0;
    pthread_mutex_lock(&q.lock);
    if (q.used + len <= q.capacity) {
        Node *n = malloc(sizeof(Node));
        n->data = malloc(len);
        memcpy(n->data, data, len);
        n->len = len;
        n->next = NULL;
        if (q.tail) q.tail->next = n;
        else q.head = n;
        q.tail = n;
        q.used += len;
        ok = 1;
    }
    pthread_mutex_unlock(&q.lock);
    return ok;
}

/* returns malloc'd data (caller frees) and sets *len, NULL if empty */
char *queue_pop(int *len) {
    char *data = NULL;
    pthread_mutex_lock(&q.lock);
    if (q.head) {
        Node *n = q.head;
        q.head = n->next;
        if (!q.head) q.tail = NULL;
        data = n->data;
        *len = n->len;
        q.used -= n->len;
        free(n);
    }
    pthread_mutex_unlock(&q.lock);
    return data;
}

void queue_destroy(void) {
    pthread_mutex_lock(&q.lock);
    Node *cur = q.head;
    while (cur) {
        Node *next = cur->next;
        free(cur->data);
        free(cur);
        cur = next;
    }
    q.head = q.tail = NULL;
    q.used = 0;
    pthread_mutex_unlock(&q.lock);
    pthread_mutex_destroy(&q.lock);
}

void handle_shutdown(int sig) {
    (void)sig;
    printf("\nShutting down server, freeing queue memory...\n");
    queue_destroy();
    close(server_fd);
    exit(0);
}

/* ---- Per-connection worker thread ---- */
void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    char type;
    if (read_full(client_fd, &type, 1) < 0) { close(client_fd); return NULL; }

    if (type == 'P') {                         /* Producer request */
        uint32_t netlen;
        if (read_full(client_fd, &netlen, 4) < 0) { close(client_fd); return NULL; }
        int len = ntohl(netlen);
        if (len <= 0 || len > MAX_MSG_SIZE) { close(client_fd); return NULL; }

        char *buf = malloc(len);
        if (read_full(client_fd, buf, len) < 0) { free(buf); close(client_fd); return NULL; }

        int ok = queue_push(buf, len);
        free(buf);

        char resp = ok ? 1 : 0;
        write_full(client_fd, &resp, 1);
        printf("[PRODUCER] %s (used=%ld/%ld bytes)\n",
               ok ? "message enqueued" : "queue FULL, rejected", q.used, q.capacity);
    }
    else if (type == 'C') {                    /* Consumer request */
        int len;
        char *data = queue_pop(&len);
        if (!data) {
            char resp = 0;
            write_full(client_fd, &resp, 1);
            printf("[CONSUMER] queue EMPTY, rejected\n");
        } else {
            char resp = 1;
            write_full(client_fd, &resp, 1);
            uint32_t netlen = htonl(len);
            write_full(client_fd, &netlen, 4);
            write_full(client_fd, data, len);
            printf("[CONSUMER] message dequeued (used=%ld/%ld bytes)\n", q.used, q.capacity);
            free(data);
        }
    }

    close(client_fd);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <queue_size_in_KB>\n", argv[0]);
        exit(1);
    }
    long kb = atol(argv[1]);
    if (kb <= 0) { fprintf(stderr, "Invalid queue size\n"); exit(1); }
    queue_init(kb * 1024);

    signal(SIGINT, handle_shutdown);
    signal(SIGTERM, handle_shutdown);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); exit(1);
    }
    listen(server_fd, 50);

    printf("Server started. Queue capacity = %ld KB. Listening on port %d\n", kb, PORT);

    while (1) {
        int *client_fd = malloc(sizeof(int));
        *client_fd = accept(server_fd, NULL, NULL);
        if (*client_fd < 0) { free(client_fd); continue; }

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, client_fd);
        pthread_detach(tid);
    }

    return 0;
}