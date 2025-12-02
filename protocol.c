#include "protocol.h"
#include <string.h>
#include <arpa/inet.h>

int serialize_message(const Message *msg, uint8_t *buffer, size_t buffer_size) {
    if (buffer_size < sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint16_t) + msg->payload_len) {
        return -1;
    }

    size_t offset = 0;

    // Magic number (network byte order)
    uint16_t magic_net = htons(msg->magic);
    memcpy(buffer + offset, &magic_net, sizeof(uint16_t));
    offset += sizeof(uint16_t);

    // Type
    buffer[offset] = msg->type;
    offset += sizeof(uint8_t);

    // Sequence number (network byte order)
    uint32_t seq_net = htonl(msg->seq_num);
    memcpy(buffer + offset, &seq_net, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Payload length (network byte order)
    uint16_t len_net = htons(msg->payload_len);
    memcpy(buffer + offset, &len_net, sizeof(uint16_t));
    offset += sizeof(uint16_t);

    // Payload
    if (msg->payload_len > 0) {
        memcpy(buffer + offset, msg->payload, msg->payload_len);
        offset += msg->payload_len;
    }

    return (int)offset;
}

int deserialize_message(const uint8_t *buffer, size_t buffer_len, Message *msg) {
    if (buffer_len < sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint16_t)) {
        return -1;
    }

    size_t offset = 0;

    // Magic number
    uint16_t magic_net;
    memcpy(&magic_net, buffer + offset, sizeof(uint16_t));
    msg->magic = ntohs(magic_net);
    offset += sizeof(uint16_t);

    if (msg->magic != MAGIC_NUMBER) {
        return -1;  // Invalid magic number
    }

    // Type
    msg->type = buffer[offset];
    offset += sizeof(uint8_t);

    // Sequence number
    uint32_t seq_net;
    memcpy(&seq_net, buffer + offset, sizeof(uint32_t));
    msg->seq_num = ntohl(seq_net);
    offset += sizeof(uint32_t);

    // Payload length
    uint16_t len_net;
    memcpy(&len_net, buffer + offset, sizeof(uint16_t));
    msg->payload_len = ntohs(len_net);
    offset += sizeof(uint16_t);

    // Validate payload length
    if (msg->payload_len > MAX_PAYLOAD_SIZE || offset + msg->payload_len > buffer_len) {
        return -1;
    }

    // Payload
    if (msg->payload_len > 0) {
        memcpy(msg->payload, buffer + offset, msg->payload_len);
    }
    msg->payload[msg->payload_len] = '\0';  // Null terminate

    return 0;
}

void create_data_message(Message *msg, uint32_t seq_num, const char *payload) {
    msg->magic = MAGIC_NUMBER;
    msg->type = MSG_TYPE_DATA;
    msg->seq_num = seq_num;
    msg->payload_len = (uint16_t)strlen(payload);

    if (msg->payload_len > MAX_PAYLOAD_SIZE) {
        msg->payload_len = MAX_PAYLOAD_SIZE;
    }

    memcpy(msg->payload, payload, msg->payload_len);
    msg->payload[msg->payload_len] = '\0';
}

void create_ack_message(Message *msg, uint32_t seq_num) {
    msg->magic = MAGIC_NUMBER;
    msg->type = MSG_TYPE_ACK;
    msg->seq_num = seq_num;
    msg->payload_len = 0;
    msg->payload[0] = '\0';
}