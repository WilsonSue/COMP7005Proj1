#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>

void log_client(FILE *log_fp, const char *format, ...) {
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

int parse_client_args(int argc, char *argv[], ClientConfig *config) {
    // Set defaults
    config->target_ip = NULL;
    config->target_port = 0;
    config->timeout = 2.0;
    config->max_retries = 5;
    config->log_file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--target-ip") == 0 && i + 1 < argc) {
            config->target_ip = argv[++i];
        } else if (strcmp(argv[i], "--target-port") == 0 && i + 1 < argc) {
            config->target_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc) {
            config->timeout = atof(argv[++i]);
        } else if (strcmp(argv[i], "--max-retries") == 0 && i + 1 < argc) {
            config->max_retries = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--log-file") == 0 && i + 1 < argc) {
            config->log_file = argv[++i];
        }
    }

    if (!config->target_ip || config->target_port == 0) {
        fprintf(stderr, "Usage: %s --target-ip <ip> --target-port <port> [--timeout <sec>] [--max-retries <n>] [--log-file <file>]\n", argv[0]);
        return -1;
    }

    return 0;
}

int create_udp_socket(void) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }
    return sockfd;
}

int send_message_with_retry(int sockfd, struct sockaddr_in *server_addr,
                            const char *payload, uint32_t seq_num,
                            const ClientConfig *config, FILE *log_fp) {
    Message msg, ack;
    uint8_t buffer[1024];
    int attempts = 0;
    struct timeval tv;
    fd_set readfds;

    create_data_message(&msg, seq_num, payload);

    while (attempts < config->max_retries) {
        // Serialize and send message
        int msg_len = serialize_message(&msg, buffer, sizeof(buffer));
        if (msg_len < 0) {
            log_client(log_fp, "ERROR: Failed to serialize message");
            return -1;
        }

        ssize_t sent = sendto(sockfd, buffer, msg_len, 0,
                             (struct sockaddr *)server_addr, sizeof(*server_addr));
        if (sent < 0) {
            log_client(log_fp, "ERROR: sendto failed: %s", strerror(errno));
            return -1;
        }

        log_client(log_fp, "SEND: seq=%u, attempt=%d, payload=\"%s\"",
                  seq_num, attempts + 1, payload);

        // Wait for ACK with timeout
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        tv.tv_sec = (long)config->timeout;
        tv.tv_usec = (long)((config->timeout - tv.tv_sec) * 1000000);

        int ready = select(sockfd + 1, &readfds, NULL, NULL, &tv);

        if (ready < 0) {
            log_client(log_fp, "ERROR: select failed: %s", strerror(errno));
            return -1;
        } else if (ready == 0) {
            // Timeout
            log_client(log_fp, "TIMEOUT: seq=%u, attempt=%d", seq_num, attempts + 1);
            attempts++;
            continue;
        }

        // Receive ACK
        struct sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);
        ssize_t recv_len = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                                    (struct sockaddr *)&from_addr, &from_len);

        if (recv_len < 0) {
            log_client(log_fp, "ERROR: recvfrom failed: %s", strerror(errno));
            return -1;
        }

        // Deserialize ACK
        if (deserialize_message(buffer, recv_len, &ack) < 0) {
            log_client(log_fp, "ERROR: Failed to deserialize ACK");
            attempts++;
            continue;
        }

        if (ack.type == MSG_TYPE_ACK && ack.seq_num == seq_num) {
            log_client(log_fp, "ACK_RECV: seq=%u", seq_num);
            return 0;  // Success
        } else {
            log_client(log_fp, "WARN: Unexpected ACK seq=%u (expected %u)",
                      ack.seq_num, seq_num);
            attempts++;
        }
    }

    log_client(log_fp, "FAILED: seq=%u after %d attempts", seq_num, config->max_retries);
    return -1;
}

int main(int argc, char *argv[]) {
    ClientConfig config;
    FILE *log_fp = NULL;

    if (parse_client_args(argc, argv, &config) < 0) {
        return EXIT_FAILURE;
    }

    if (config.log_file) {
        log_fp = fopen(config.log_file, "a");
        if (!log_fp) {
            fprintf(stderr, "Warning: Could not open log file %s\n", config.log_file);
        }
    }

    log_client(log_fp, "CLIENT STARTED: target=%s:%d, timeout=%.1fs, max_retries=%d",
              config.target_ip, config.target_port, config.timeout, config.max_retries);

    int sockfd = create_udp_socket();
    if (sockfd < 0) {
        if (log_fp) fclose(log_fp);
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config.target_port);

    if (inet_pton(AF_INET, config.target_ip, &server_addr.sin_addr) <= 0) {
        log_client(log_fp, "ERROR: Invalid target IP address");
        close(sockfd);
        if (log_fp) fclose(log_fp);
        return EXIT_FAILURE;
    }

    char line[MAX_PAYLOAD_SIZE + 1];
    uint32_t seq_num = 0;

    printf("Enter messages (Ctrl+D to quit):\n");

    while (fgets(line, sizeof(line), stdin)) {
        // Remove newline
        line[strcspn(line, "\n")] = '\0';

        if (strlen(line) == 0) {
            continue;
        }

        if (send_message_with_retry(sockfd, &server_addr, line, seq_num,
                                    &config, log_fp) == 0) {
            printf("✓ Message sent successfully (seq=%u)\n", seq_num);
        } else {
            printf("✗ Failed to send message (seq=%u)\n", seq_num);
        }

        seq_num++;
    }

    log_client(log_fp, "CLIENT SHUTDOWN");
    close(sockfd);
    if (log_fp) fclose(log_fp);

    return EXIT_SUCCESS;
}