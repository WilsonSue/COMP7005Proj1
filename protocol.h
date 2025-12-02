#ifndef COMP7005PROJ1_PROTOCOL_H
#define COMP7005PROJ1_PROTOCOL_H


#include <stddef.h>
#include <stdint.h>

#define MAX_PAYLOAD_SIZE 512
#define MAGIC_NUMBER 0x55AA

// Message types
typedef enum {
    MSG_TYPE_DATA = 1,
    MSG_TYPE_ACK = 2
} MessageType;

// Message structure
typedef struct __attribute__((packed)) {
    uint16_t magic;           // Magic number for validation
    uint8_t type;             // Message type (DATA or ACK)
    uint32_t seq_num;         // Sequence number
    uint16_t payload_len;     // Length of payload
    char payload[MAX_PAYLOAD_SIZE];  // Actual message data
} Message;

// Function prototypes
int serialize_message(const Message *msg, uint8_t *buffer, size_t buffer_size);
int deserialize_message(const uint8_t *buffer, size_t buffer_len, Message *msg);
void create_data_message(Message *msg, uint32_t seq_num, const char *payload);
void create_ack_message(Message *msg, uint32_t seq_num);

#endif //COMP7005PROJ1_PROTOCOL_H