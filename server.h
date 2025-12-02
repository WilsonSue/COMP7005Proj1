#ifndef COMP7005PROJ1_SERVER_H
#define COMP7005PROJ1_SERVER_H

#include <stdio.h>
#include "protocol.h"
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct {
    char *listen_ip;
    int listen_port;
    char *log_file;
} ServerConfig;

// Function prototypes
int parse_server_args(int argc, char *argv[], ServerConfig *config);
int create_and_bind_udp_socket(const char *ip, int port);
void handle_message(int sockfd, FILE *log_fp);
void log_server(FILE *log_fp, const char *format, ...);

#endif //COMP7005PROJ1_SERVER_H