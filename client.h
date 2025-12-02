#ifndef COMP7005PROJ1_CLIENT_H
#define COMP7005PROJ1_CLIENT_H

#include <stdio.h>
#include "protocol.h"
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct {
    char *target_ip;
    int target_port;
    double timeout;
    int max_retries;
    char *log_file;
} ClientConfig;

// Function prototypes
int parse_client_args(int argc, char *argv[], ClientConfig *config);
int create_udp_socket(void);
int send_message_with_retry(int sockfd, struct sockaddr_in *server_addr,
                            const char *payload, uint32_t seq_num,
                            const ClientConfig *config, FILE *log_fp);
void log_client(FILE *log_fp, const char *format, ...);


#endif //COMP7005PROJ1_CLIENT_H