#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/types.h>

int main(void) {
    int listen_fd, client_fd;
    struct sockaddr_in serv_addr;
    unsigned char buffer[1024];
    ssize_t n;

    // Create a TCP socket
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set SO_REUSEADDR to allow reuse of the address
    int optval = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt(SO_REUSEADDR)");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    // Prepare the server address structure
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(4000);

    // Convert IP address from text to binary form
    if (inet_pton(AF_INET, "192.0.3.1", &serv_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the given IP and port
    if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    // Start listening for connections
    if (listen(listen_fd, 1) < 0) {
        perror("listen");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "Server is listening on 192.0.3.1:4000...\n");

    // Accept a single client connection
    client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd < 0) {
        perror("accept");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    // Enable TCP keepalive
    optval = 1;
    if (setsockopt(client_fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0) {
        perror("setsockopt(SO_KEEPALIVE)");
        close(client_fd);
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    // Set TCP_KEEPIDLE = 5 seconds
    optval = 5;
    if (setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPIDLE, &optval, sizeof(optval)) < 0) {
        perror("setsockopt(TCP_KEEPIDLE)");
        close(client_fd);
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    // Set TCP_KEEPINTVL = 1 second
    optval = 1;
    if (setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPINTVL, &optval, sizeof(optval)) < 0) {
        perror("setsockopt(TCP_KEEPINTVL)");
        close(client_fd);
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    // Set TCP_KEEPCNT = 4 probes
    optval = 4;
    if (setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPCNT, &optval, sizeof(optval)) < 0) {
        perror("setsockopt(TCP_KEEPCNT)");
        close(client_fd);
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "Client connected, receiving data...\n");

    while ((n = recv(client_fd, buffer, sizeof(buffer), 0)) > 0) {
        // Print received data in hex format
        for (int i = 0; i < n; i++) {
            printf("0x%02x ", buffer[i]);
        }
        printf("\n");
        fflush(stdout);
    }

    if (n < 0) {
        perror("recv");
    } else {
        fprintf(stderr, "Client disconnected.\n");
    }

    // Close the client and the listening socket
    close(client_fd);
    close(listen_fd);

    return 0;
}
