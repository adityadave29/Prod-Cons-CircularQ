#include "common.h"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr,"Usage: %s \"message text\"\n",argv[0]);
        exit(1);
    }

    const char *message = argv[1];
    size_t len = strlen(message);

    if (len == 0 || len > MAX_MSG_SIZE) {
        fprintf(stderr,"Invalid message size\n");
        exit(1);
    }


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
     * Protocol:
     *
     * [P][4-byte length][message]
     */

    char type = 'P';
    write_full(sock,&type,1);


    uint32_t net_len = htonl((uint32_t)len);
    write_full(sock,&net_len,sizeof(net_len));
    write_full(sock,message,len);


    /* Wait for server response */
    char response;
    if (read_full(sock,&response,1) < 0) {
        close(sock);
        exit(1);
    }
    close(sock);
    if (response == 1) {
        printf("Producer: message \"%s\" ""added successfully.\n", message);
        return 0;
    }


    printf("Producer: queue is FULL. ""Terminating producer.\n");
    return 1;
}