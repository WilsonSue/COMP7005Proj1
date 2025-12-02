#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sys/select.h>


static volatile int running = 1;

void sigint_handler(int sig) {
    (void)sig;
    running = 0;
}

void log_server(FILE *log_fp, const char *format, ...) {
    va_list args;
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    // Print to stderr
    fprintf(stderr, "[%s] ", timestamp);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");

    // Print to log file if provided
    if (log_fp) {
        fprintf(log_fp, "[%s] ", timestamp);
        va_start(args, format);
        vfprintf(log_fp, format, args);
        va_end(args);
        fprintf(log_fp, "\n");
        fflush(log_fp);
    }
}

int parse_server_args(int argc, char *argv[], ServerConfig *config) {
    config->listen_ip = NULL;
    config->listen_port = 0;
    config->log_file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--listen-ip") == 0 && i + 1 < argc) {
            config->listen_ip = argv[++i];
        } else if (strcmp(argv[i], "--listen-port") == 0 && i + 1 < argc) {
            config->listen_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--log-file") == 0 && i + 1 < argc) {
            config->log_file = argv[++i];
        }
    }

    if (!config->listen_ip || config->listen_port == 0) {
        fprintf(stderr, "Usage: %s --listen-ip <ip> --listen-port <port> [--log-file <file>]\n", argv[0]);
        return -1;
    }

    return 0;
}

int create_and_bind_udp_socket(const char *ip, int port) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt");
        close(sockfd);
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid IP address\n");
        close(sockfd);
        return -1;
    }

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

void handle_message(int sockfd, FILE *log_fp) {
    uint8_t buffer[1024];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    ssize_t recv_len = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                                (struct sockaddr *)&client_addr, &client_len);

    if (recv_len < 0) {
        log_server(log_fp, "ERROR: recvfrom failed: %s", strerror(errno));
        return;
    }

    // Deserialize message
    Message msg;
    if (deserialize_message(buffer, recv_len, &msg) < 0) {
        log_server(log_fp, "ERROR: Failed to deserialize message");
        return;
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

    if (msg.type == MSG_TYPE_DATA) {
        log_server(log_fp, "RECV: seq=%u, from=%s:%d, payload=\"%s\"",
                  msg.seq_num, client_ip, ntohs(client_addr.sin_port), msg.payload);

        // Print message to stdout (as required)
        printf("Message (seq=%u): %s\n", msg.seq_num, msg.payload);
        fflush(stdout);

        // Send ACK
        Message ack;
        create_ack_message(&ack, msg.seq_num);

        int ack_len = serialize_message(&ack, buffer, sizeof(buffer));
        if (ack_len < 0) {
            log_server(log_fp, "ERROR: Failed to serialize ACK");
            return;
        }

        ssize_t sent = sendto(sockfd, buffer, ack_len, 0,
                             (struct sockaddr *)&client_addr, client_len);
        if (sent < 0) {
            log_server(log_fp, "ERROR: sendto ACK failed: %s", strerror(errno));
            return;
        }

        log_server(log_fp, "ACK_SEND: seq=%u, to=%s:%d",
                  msg.seq_num, client_ip, ntohs(client_addr.sin_port));
    } else {
        log_server(log_fp, "WARN: Unexpected message type %d", msg.type);
    }
}

int main(int argc, char *argv[]) {
    ServerConfig config;
    FILE *log_fp = NULL;

    if (parse_server_args(argc, argv, &config) < 0) {
        return EXIT_FAILURE;
    }

    if (config.log_file) {
        log_fp = fopen(config.log_file, "a");
        if (!log_fp) {
            fprintf(stderr, "Warning: Could not open log file %s\n", config.log_file);
        }
    }

    signal(SIGINT, sigint_handler);

    log_server(log_fp, "SERVER STARTED: listening on %s:%d",
              config.listen_ip, config.listen_port);

    int sockfd = create_and_bind_udp_socket(config.listen_ip, config.listen_port);
    if (sockfd < 0) {
        if (log_fp) fclose(log_fp);
        return EXIT_FAILURE;
    }

    printf("Server listening on %s:%d\n", config.listen_ip, config.listen_port);
    printf("Press Ctrl+C to stop\n\n");

    while (running) {
        fd_set readfds;
        struct timeval tv;

        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ready = select(sockfd + 1, &readfds, NULL, NULL, &tv);

        if (ready < 0 && errno != EINTR) {
            log_server(log_fp, "ERROR: select failed: %s", strerror(errno));
            break;
        }

        if (ready > 0 && FD_ISSET(sockfd, &readfds)) {
            handle_message(sockfd, log_fp);
        }
    }

    log_server(log_fp, "SERVER SHUTDOWN");
    close(sockfd);
    if (log_fp) fclose(log_fp);

    return EXIT_SUCCESS;
}