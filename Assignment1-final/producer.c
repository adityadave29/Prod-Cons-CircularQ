#include "common.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s \"message text\"\n", argv[0]);
        exit(1);
    }
    const char *msg = argv[1];
    int len = strlen(msg);

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

    char type = 'P';
    write_full(sock, &type, 1);
    uint32_t netlen = htonl(len);
    write_full(sock, &netlen, 4);
    write_full(sock, msg, len);

    char resp;
    if (read_full(sock, &resp, 1) < 0) { close(sock); exit(1); }
    close(sock);

    if (resp == 1) {
        printf("Producer: message \"%s\" added to queue successfully.\n", msg);
        return 0;
    } else {
        printf("Producer: queue is FULL. Terminating producer.\n");
        exit(1);
    }
}