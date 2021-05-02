// Copyright 2021 Pescaru Tudor-Mihai 321CA
#ifndef PROTOCOL_H_
#define PROTOCOL_H_ 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

// Error checking macro
#define DIE(assertion, call_description)    \
    do {                                    \
        if (assertion) {                    \
            fprintf(stderr, "(%s, %d): ",   \
                    __FILE__, __LINE__);    \
            perror(call_description);       \
            exit(EXIT_FAILURE);             \
        }                                   \
    } while (0)

// Define length of UDP packet fields
#define TOPIC_LEN 50
#define CONTENT_LEN 1500

// Define length of client ID
#define ID_LEN 10

// Define protocol message codes
#define EXIT_MSG 0
#define CONF_MSG 1
#define PKT_MSG 2
#define ID_MSG 3
#define SUB_MSG 4
#define UNSUB_MSG 5
#define ERR_MSG 6

// Define max client request buffer for TCP socket listen
#define MAX_CLIENTS 10

// Define macros for command lengths
#define EXIT_LEN strlen("exit")
#define SUB_LEN strlen("subscribe")
#define UNSUB_LEN strlen("unsubscribe")

// Define structure for packet received from UDP clients
struct udp_packet {
    char topic[TOPIC_LEN];
    uint8_t data_type;
    char content[CONTENT_LEN];
};

#define UDPPACKET_LEN (sizeof(struct udp_packet))

// Define structure for packet to be sent to TCP clients
struct packet {
    struct in_addr udp_client_ip;
    uint16_t udp_client_port;
    struct udp_packet udp_msg;
};

#define PACKET_LEN (sizeof(struct packet))

// Define structure used in sending a subscribe/unsubscribe message to server
struct subs_packet {
    char topic[TOPIC_LEN];
    bool sf;
};

#define SUBSPACKET_LEN (sizeof(struct subs_packet))

// Define structure used in announcing a TCP client of an incoming packet and
// sizes of packet and contents
struct size_packet {
    size_t to_receive;
    size_t topic_size;
    size_t content_size;
};

#define SIZEPACKET_LEN (sizeof(struct size_packet))

// Define structure for standard protocol message
struct proto_msg {
    uint8_t msg_type;
    char payload[SUBSPACKET_LEN];
};

#define MSG_LEN (sizeof(struct proto_msg))

#endif  // PROTOCOL_H_
