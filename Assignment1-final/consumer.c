#include "common.h"

int main(void) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect (is the server running?)");
        exit(1);
    }

    char type = 'C';
    write_full(sock, &type, 1);

    char resp;
    if (read_full(sock, &resp, 1) < 0) { close(sock); exit(1); }

    if (resp == 0) {
        printf("Consumer: queue is EMPTY. Terminating consumer.\n");
        close(sock);
        exit(1);
    }

    uint32_t netlen;
    read_full(sock, &netlen, 4);
    int len = ntohl(netlen);
    char *buf = malloc(len + 1);
    read_full(sock, buf, len);   /* read the complete message chunk */
    buf[len] = '\0';

    printf("Consumer: received message -> \"%s\"\n", buf);
    free(buf);
    close(sock);
    return 0;
}