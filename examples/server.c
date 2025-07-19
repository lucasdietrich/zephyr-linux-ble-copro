// gcc -Wall -O2 -o server server.c

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define ADDR "192.0.3.1"
#define PORT 4000
#define KEEP_ALIVE_IDLE  5   /* seconds before first probe  */
#define KEEP_ALIVE_INTVL 1   /* seconds between probes      */
#define KEEP_ALIVE_CNT   1   /* probes before giving up     */

static void set_keepalive(int fd)
{
    int optval = 1;
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval);
    optval = KEEP_ALIVE_IDLE;
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE,  &optval,  sizeof(int));
    optval = KEEP_ALIVE_INTVL;
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &optval, sizeof(int));
    optval = KEEP_ALIVE_CNT;
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT,   &optval,   sizeof(int));
}

int main(void)
{
    /* create listening socket */
    int lsock = socket(AF_INET, SOCK_STREAM, 0);
    if (lsock < 0) { perror("socket"); exit(EXIT_FAILURE); }

    int optval = 1;
    setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(PORT),
        .sin_addr.s_addr = inet_addr(ADDR)
    };
    if (bind(lsock, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("bind"); exit(EXIT_FAILURE);
    }
    if (listen(lsock, 1) < 0) {
        perror("listen"); exit(EXIT_FAILURE);
    }

    printf("Server listening on %s:%d\n", ADDR, PORT);

    for (;;) {
        int csock = accept(lsock, NULL, NULL);
        if (csock < 0) {
            perror("accept");
            continue;
        }
        printf("Client connected\n");

        set_keepalive(csock);

        char buf[512];
        ssize_t n;
        while ((n = read(csock, buf, sizeof buf)) > 0) {
            printf("Received %zd bytes\n", n);
        }

        if (n == 0) {
            printf("Client closed the connection\n");
        } else {
            fprintf(stderr, "read error: %s\n", strerror(errno));
        }

        close(csock);
    }
}
