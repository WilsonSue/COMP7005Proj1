#ifndef COMP7005PROJ1_PROXY_H
#define COMP7005PROJ1_PROXY_H

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct {
    char *listen_ip;
    int listen_port;
    char *target_ip;
    int target_port;
    int client_drop;           // Drop percentage for client->server
    int server_drop;           // Drop percentage for server->client
    int client_delay;          // Delay percentage for client->server
    int server_delay;          // Delay percentage for server->client
    int client_delay_min;      // Min delay in ms
    int client_delay_max;      // Max delay in ms
    int server_delay_min;      // Min delay in ms
    int server_delay_max;      // Max delay in ms
    char *log_file;
} ProxyConfig;

// Function prototypes
int parse_proxy_args(int argc, char *argv[], ProxyConfig *config);
int create_and_bind_udp_socket(const char *ip, int port);
int should_drop(int drop_percentage);
int get_delay_ms(int delay_percentage, int min_ms, int max_ms);
void log_proxy(FILE *log_fp, const char *format, ...);

#endif //COMP7005PROJ1_PROXY_H