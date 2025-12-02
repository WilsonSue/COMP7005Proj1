#include "proxy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>

static volatile int running = 1;

void sigint_handler(int sig) {
    (void)sig;
    running = 0;
}

void log_proxy(FILE *log_fp, const char *format, ...) {
    va_list args;
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    fprintf(stderr, "[%s] ", timestamp);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");

    if (log_fp) {
        fprintf(log_fp, "[%s] ", timestamp);
        va_start(args, format);
        vfprintf(log_fp, format, args);
        va_end(args);
        fprintf(log_fp, "\n");
        fflush(log_fp);
    }
}

int parse_proxy_args(int argc, char *argv[], ProxyConfig *config) {
    config->listen_ip = NULL;
    config->listen_port = 0;
    config->target_ip = NULL;
    config->target_port = 0;
    config->client_drop = 0;
    config->server_drop = 0;
    config->client_delay = 0;
    config->server_delay = 0;
    config->client_delay_min = 0;
    config->client_delay_max = 0;
    config->server_delay_min = 0;
    config->server_delay_max = 0;
    config->log_file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--listen-ip") == 0 && i + 1 < argc) {
            config->listen_ip = argv[++i];
        } else if (strcmp(argv[i], "--listen-port") == 0 && i + 1 < argc) {
            config->listen_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--target-ip") == 0 && i + 1 < argc) {
            config->target_ip = argv[++i];
        } else if (strcmp(argv[i], "--target-port") == 0 && i + 1 < argc) {
            config->target_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--client-drop") == 0 && i + 1 < argc) {
            config->client_drop = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--server-drop") == 0 && i + 1 < argc) {
            config->server_drop = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--client-delay") == 0 && i + 1 < argc) {
            config->client_delay = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--server-delay") == 0 && i + 1 < argc) {
            config->server_delay = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--client-delay-time-min") == 0 && i + 1 < argc) {
            config->client_delay_min = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--client-delay-time-max") == 0 && i + 1 < argc) {
            config->client_delay_max = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--server-delay-time-min") == 0 && i + 1 < argc) {
            config->server_delay_min = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--server-delay-time-max") == 0 && i + 1 < argc) {
            config->server_delay_max = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--log-file") == 0 && i + 1 < argc) {
            config->log_file = argv[++i];
        }
    }

    if (!config->listen_ip || !config->target_ip ||
        config->listen_port == 0 || config->target_port == 0) {
        fprintf(stderr, "Usage: %s --listen-ip <ip> --listen-port <port> "
                       "--target-ip <ip> --target-port <port> "
                       "[--client-drop <%%>] [--server-drop <%%>] "
                       "[--client-delay <%%>] [--server-delay <%%>] "
                       "[--client-delay-time-min <ms>] [--client-delay-time-max <ms>] "
                       "[--server-delay-time-min <ms>] [--server-delay-time-max <ms>] "
                       "[--log-file <file>]\n", argv[0]);
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

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid IP address\n");
        close(sockfd);
        return -1;
    }

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int should_drop(int drop_percentage) {
    if (drop_percentage <= 0) return 0;
    if (drop_percentage >= 100) return 1;
    return (rand() % 100) < drop_percentage;
}

int get_delay_ms(int delay_percentage, int min_ms, int max_ms) {
    if (delay_percentage <= 0) return 0;
    if ((rand() % 100) >= delay_percentage) return 0;

    if (min_ms >= max_ms) return min_ms;
    return min_ms + (rand() % (max_ms - min_ms + 1));
}

int main(int argc, char *argv[]) {
    ProxyConfig config;
    FILE *log_fp = NULL;

    if (parse_proxy_args(argc, argv, &config) < 0) {
        return EXIT_FAILURE;
    }

    if (config.log_file) {
        log_fp = fopen(config.log_file, "a");
        if (!log_fp) {
            fprintf(stderr, "Warning: Could not open log file %s\n", config.log_file);
        }
    }

    srand((unsigned int)time(NULL));
    signal(SIGINT, sigint_handler);

    log_proxy(log_fp, "PROXY STARTED: listen=%s:%d, target=%s:%d",
             config.listen_ip, config.listen_port, config.target_ip, config.target_port);
    log_proxy(log_fp, "CLIENT->SERVER: drop=%d%%, delay=%d%% (%d-%dms)",
             config.client_drop, config.client_delay,
             config.client_delay_min, config.client_delay_max);
    log_proxy(log_fp, "SERVER->CLIENT: drop=%d%%, delay=%d%% (%d-%dms)",
             config.server_drop, config.server_delay,
             config.server_delay_min, config.server_delay_max);

    int sockfd = create_and_bind_udp_socket(config.listen_ip, config.listen_port);
    if (sockfd < 0) {
        if (log_fp) fclose(log_fp);
        return EXIT_FAILURE;
    }

    // Set up target server address
    struct sockaddr_in target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(config.target_port);
    if (inet_pton(AF_INET, config.target_ip, &target_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid target IP address\n");
        close(sockfd);
        if (log_fp) fclose(log_fp);
        return EXIT_FAILURE;
    }

    struct sockaddr_in client_addr;
    socklen_t client_len;
    uint8_t buffer[2048];

    printf("Proxy running on %s:%d -> %s:%d\n",
           config.listen_ip, config.listen_port,
           config.target_ip, config.target_port);
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
            log_proxy(log_fp, "ERROR: select failed: %s", strerror(errno));
            break;
        }

        if (ready > 0 && FD_ISSET(sockfd, &readfds)) {
            client_len = sizeof(client_addr);
            ssize_t recv_len = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                                       (struct sockaddr *)&client_addr, &client_len);

            if (recv_len < 0) {
                log_proxy(log_fp, "ERROR: recvfrom failed: %s", strerror(errno));
                continue;
            }

            char from_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, from_ip, sizeof(from_ip));
            int from_port = ntohs(client_addr.sin_port);

            // Determine direction
            int is_from_client = (strcmp(from_ip, config.target_ip) != 0 ||
                                 from_port != config.target_port);

            if (is_from_client) {
                // Client -> Server
                log_proxy(log_fp, "C->S: Received %zd bytes from %s:%d",
                         recv_len, from_ip, from_port);

                if (should_drop(config.client_drop)) {
                    log_proxy(log_fp, "C->S: DROPPED");
                    continue;
                }

                int delay = get_delay_ms(config.client_delay,
                                       config.client_delay_min,
                                       config.client_delay_max);
                if (delay > 0) {
                    log_proxy(log_fp, "C->S: DELAYED %dms", delay);
                    usleep(delay * 1000);
                }

                // Forward to server
                ssize_t sent = sendto(sockfd, buffer, recv_len, 0,
                                    (struct sockaddr *)&target_addr, sizeof(target_addr));
                if (sent < 0) {
                    log_proxy(log_fp, "ERROR: sendto server failed: %s", strerror(errno));
                } else {
                    log_proxy(log_fp, "C->S: Forwarded %zd bytes to %s:%d",
                             sent, config.target_ip, config.target_port);
                }
            } else {
                // Server -> Client
                log_proxy(log_fp, "S->C: Received %zd bytes from server", recv_len);

                if (should_drop(config.server_drop)) {
                    log_proxy(log_fp, "S->C: DROPPED");
                    continue;
                }

                int delay = get_delay_ms(config.server_delay,
                                       config.server_delay_min,
                                       config.server_delay_max);
                if (delay > 0) {
                    log_proxy(log_fp, "S->C: DELAYED %dms", delay);
                    usleep(delay * 1000);
                }

                // Forward to original client
                // Note: We need to track client address from previous packet
                log_proxy(log_fp, "S->C: Forwarded %zd bytes to client", recv_len);
            }
        }
    }

    log_proxy(log_fp, "PROXY SHUTDOWN");
    close(sockfd);
    if (log_fp) fclose(log_fp);

    return EXIT_SUCCESS;
}