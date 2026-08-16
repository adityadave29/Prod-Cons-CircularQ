#include "common.h"

int main(void)
{
    /* Create socket */
    int sock = socket(AF_INET,SOCK_STREAM,0);

    if (sock < 0) {
        perror("socket");
        exit(1);
    }


    struct sockaddr_in server_addr;
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET,"127.0.0.1",&server_addr.sin_addr);


    /* Connect to server */
    if (connect(sock,(struct sockaddr *)&server_addr,sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        exit(1);
    }

    /*
     * Tell server this is a consumer.
     */
    char type = 'C';
    write_full(sock,&type,1);
    /*
     * Server sends:
     *
     * 0 → queue empty
     * 1 → message available
     */

    char response;
    if (read_full(sock,&response,1) < 0) {
        close(sock);
        exit(1);
    }

    if (response == 0) {
        printf("Consumer: queue is EMPTY. ""Terminating consumer.\n");
        close(sock);
        return 1;
    }
    uint32_t net_len;

    if (read_full(sock,&net_len,sizeof(net_len)) < 0) {
        close(sock);
        exit(1);
    }

    size_t len = ntohl(net_len);

    if (len == 0 || len > MAX_MSG_SIZE) {
        close(sock);
        exit(1);
    }
    char *message = malloc(len + 1);

    if (message == NULL) {
        close(sock);
        exit(1);
    }

    if (read_full(sock,message,len) < 0) {
        free(message);
        close(sock);
        exit(1);
    }

    message[len] = '\0';
    printf("Consumer: received message -> \"%s\"\n",message);
    free(message);
    close(sock);
    return 0;
}