// Copyright 2021 Pescaru Tudor-Mihai 321CA
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <iostream>
#include <iomanip>
#include <unordered_map>
#include <string>

#include "./protocol.h"

int main(int argc, char **argv) {
    // Disable STDOUT buffering
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    // Enable or disable debugging mode messages
    bool debug = true;

    // Check if correct number of args was given
    if (argc != 4) {
        if (debug) {
            std::cerr << "Usage: ";
            std::cerr << "./subscriber <CLIENT_ID> <SERVER_IP> <SERVER_PORT>\n";
        }
        exit(0);
    }

    int sockfd, portno, ret;
    struct sockaddr_in serv_addr;
    char buffer[PACKET_LEN];
    bool exit_status = false;

    // Use a set to store subscribed topics for client-side checks
    std::unordered_map<std::string, bool> subscribed;

    fd_set read_fds;
    fd_set tmp_fds;
    int fdmax;

    // Initialize file descriptor sets
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    // Create socket for TCP communication with the server
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket");

    // Disable Neagle's algorithm
    int optval = 1;
    ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY,
                    reinterpret_cast<void *>(&optval), sizeof(optval));
    DIE(ret < 0, "set TCP_NODELAY");
    // Enable port reusage
    ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                    reinterpret_cast<void *>(&optval), sizeof(optval));
    DIE(ret < 0, "set SO_REUSEADDR for TCP");

    // Convert given port number to int
    portno = atoi(argv[3]);
    DIE(portno == 0, "port atoi");

    // Set IP and port to connect to server
    memset(reinterpret_cast<char *>(&serv_addr), 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    ret = inet_aton(argv[2], &serv_addr.sin_addr);
    DIE(ret == 0, "inet_aton");

    // Establish connection to server
    ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
    DIE(ret < 0, "connect");

    // Send client ID message to server
    struct proto_msg msg;
    msg.msg_type = ID_MSG;
    memset(msg.payload, 0, SUBSPACKET_LEN);
    memcpy(msg.payload, argv[1], strlen(argv[1]));
    ret = send(sockfd, &msg, MSG_LEN, 0);
    DIE(ret < 0, "send");

    // Add STDIN and socket file descriptors to set
    FD_SET(STDIN_FILENO, &read_fds);
    FD_SET(sockfd, &read_fds);
    fdmax = sockfd;

    while (!exit_status) {
        tmp_fds = read_fds;

        // Select socket fds to read from
        ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(ret < 0, "select");

        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &tmp_fds)) {
                if (i == sockfd) {
                    // Receive protocol message from server
                    memset(buffer, 0, PACKET_LEN);
                    ret = recv(sockfd, buffer, MSG_LEN, 0);
                    DIE(ret < 0, "recv from server");

                    // Convert received buffer to message structure
                    struct proto_msg *msg = (struct proto_msg *)buffer;
                    if (msg->msg_type == EXIT_MSG) {
                        // If message was an exit message set exit state
                        exit_status = true;
                        break;
                    } else if (msg->msg_type == PKT_MSG) {
                        // If message was an incoming packet message
                        // get sizes struct from message payload
                        struct size_packet sz;
                        memcpy(&sz, msg->payload, SIZEPACKET_LEN);

                        // Receive packet using to_receive size from the struct
                        memset(buffer, 0, PACKET_LEN);
                        ret = recv(sockfd, buffer, sz.to_receive, 0);
                        DIE(ret < 0, "recv from server");

                        // Split received buffer into components and place in
                        // packet struct
                        struct packet pkt;
                        memset(&pkt, 0, PACKET_LEN);
                        int offset = 0;

                        memcpy(&(pkt.udp_client_ip), buffer + offset,
                                sizeof(struct in_addr));
                        offset += sizeof(struct in_addr);
                        memcpy(&(pkt.udp_client_port), buffer + offset,
                                sizeof(uint16_t));
                        offset += sizeof(uint16_t);
                        memcpy(pkt.udp_msg.topic, buffer + offset,
                                sz.topic_size);
                        offset += sz.topic_size;
                        memcpy(&(pkt.udp_msg.data_type), buffer + offset,
                                sizeof(uint8_t));
                        offset += sizeof(uint8_t);
                        memcpy(pkt.udp_msg.content, buffer + offset,
                                sz.content_size);

                        // Print UDP client ip, port and message topic
                        std::cout << inet_ntoa(pkt.udp_client_ip)
                                    << ":" << ntohs(pkt.udp_client_port)
                                    << " - " << pkt.udp_msg.topic << " - ";

                        // Convert and print content based on data type
                        if (pkt.udp_msg.data_type == 0) {
                            // Get number from content
                            uint32_t num = *reinterpret_cast<uint32_t *>
                                    (pkt.udp_msg.content + sizeof(char));

                            // Convert to host byte order
                            num = ntohl(num);

                            // Print with sign based on sign byte
                            if (pkt.udp_msg.content[0] == 0) {
                                std::cout << "INT" << " - " << num << "\n";
                            } else {
                                std::cout << "INT" << " - " << "-" << num
                                            << "\n";
                            }
                        } else if (pkt.udp_msg.data_type == 1) {
                            // Get number from content
                            uint16_t num = *reinterpret_cast<uint16_t *>
                                            (pkt.udp_msg.content);

                            // Convert to host byte order and divide by 100
                            float converted = ntohs(num) / 100.00;

                            // Print short real with 2 decimals
                            std::cout << "SHORT_REAL" << " - "
                                        << std::setprecision(2) << std::fixed
                                        << converted << "\n";
                        } else if (pkt.udp_msg.data_type == 2) {
                            // Get number from content
                            uint32_t num = *reinterpret_cast<uint32_t *>
                                        (pkt.udp_msg.content  + sizeof(char));
                            // Convert to host byte order
                            num = ntohl(num);
                            // Get power of 10 to divide by
                            uint8_t pow = *reinterpret_cast<uint8_t *>
                                            (pkt.udp_msg.content + sizeof(char)
                                             + sizeof(uint32_t));

                            // Convert number to double and divide
                            int div = static_cast<int>(pow);
                            double converted = static_cast<double>(num);
                            for (int j = 0; j < div; j++) {
                                converted /= 10;
                            }

                            // Print with decimals and sign based on sign byte
                            if (pkt.udp_msg.content[0] == 0) {
                                std::cout << "FLOAT" << " - "
                                            << std::setprecision(div)
                                            << std::fixed << converted << "\n";
                            } else {
                                std::cout << "FLOAT" << " - " << "-"
                                     << std::setprecision(div) << std::fixed
                                     << converted << "\n";
                            }
                        } else if (pkt.udp_msg.data_type == 3) {
                            // Print directly if content is a string
                            std::cout << "STRING" << " - "
                                        << pkt.udp_msg.content << "\n";
                        } else if (debug) {
                            std::cerr << "Invalid data type!\n";
                        }
                    } else if (debug) {
                        std::cerr << "Unexpected message type received!\n";
                    }
                } else {
                    // Read command from STDIN
                    memset(buffer, 0, PACKET_LEN);
                    std::cin >> buffer;

                    if (strncmp(buffer, "exit", EXIT_LEN) == 0) {
                        // If command is exit set exit state
                        exit_status = true;
                        break;
                    } else if (strncmp(buffer, "subscribe", SUB_LEN) == 0) {
                        // If command is subscribe read topic and sf from STDIN
                        std::string topic;
                        char sf, ch;
                        bool sf_val;
                        std::cin >> topic >> sf;

                        // Clear any unread chars
                        while ((ch = std::cin.get()) != '\n' && ch != EOF) {
                            continue;
                        }

                        // Check if sf is valid and if topic size is valid
                        if ((sf != '0' && sf != '1') || topic.size() > 50) {
                            if (debug) {
                                std::cerr << "Invalid command!\n";
                            }
                            continue;
                        }

                        // Assign sf value based on read value
                        if (sf == '0') {
                            sf_val = false;
                        } else {
                            sf_val = true;
                        }

                        // Check if topic is already subscribed
                        if (subscribed.find(topic) != subscribed.end()) {
                            if (subscribed[topic] == sf_val) {
                                if (debug) {
                                    std::cerr << "Topic already subscribed!\n";
                                }
                                continue;
                            } else {
                                subscribed[topic] = sf_val;
                            }
                        } else {
                            subscribed.insert({topic, sf_val});
                        }

                        // Create protocol message to request subscribe
                        // from server
                        struct proto_msg msg;
                        memset(&msg, 0, MSG_LEN);
                        msg.msg_type = SUB_MSG;

                        // Create structure with subscription data to add
                        // to message
                        struct subs_packet subs;
                        memset(subs.topic, 0, TOPIC_LEN);
                        memcpy(subs.topic, topic.c_str(), topic.size());
                        subs.sf = sf_val;

                        // Load subscription structure into message payload
                        // and send to server
                        memcpy(msg.payload, &subs, SUBSPACKET_LEN);
                        ret = send(sockfd, &msg, MSG_LEN, 0);
                        DIE(ret < 0, "send subscribe");

                        // Receive feedback from server to confirm subscription
                        memset(buffer, 0, PACKET_LEN);
                        ret = recv(sockfd, buffer, MSG_LEN, 0);
                        DIE(ret < 0, "recv feedback");

                        // Print command feedback
                        struct proto_msg *feedback = (struct proto_msg *)buffer;
                        if (feedback->msg_type == CONF_MSG) {
                            std::cout << "Subscribed to topic.\n";
                        } else if (debug) {
                            std::cerr << "Unexpected message type received!\n";
                        }
                    } else if (strncmp(buffer, "unsubscribe", UNSUB_LEN) == 0) {
                        // If command is unsubscribe read topic from STDIN
                        std::string topic;
                        char ch;
                        std::cin >> topic;

                        // Clear any unread chars
                        while ((ch = std::cin.get()) != '\n' && ch != EOF) {
                            continue;
                        }

                        // Check if topic length is correct
                        if (topic.size() > 50) {
                            if (debug) {
                                std::cerr << "Invalid topic size!\n";
                            }
                            continue;
                        }

                        // Check if topic is subscribed to
                        if (subscribed.find(topic) == subscribed.end()) {
                            if (debug) {
                                std::cerr << "Topic not subscribed to!\n";
                            }
                            continue;
                        } else {
                            subscribed.erase(topic);
                        }

                        // Create protocol message to request unsubscribe
                        // from server
                        struct proto_msg msg;
                        memset(&msg, 0, MSG_LEN);
                        msg.msg_type = UNSUB_MSG;

                        // Create structure with unsubscription data to add
                        // to message
                        struct subs_packet subs;
                        memset(subs.topic, 0, TOPIC_LEN);
                        memcpy(subs.topic, topic.c_str(), topic.size());
                        subs.sf = 0;

                        // Load unsubscription structure into message payload
                        // and send to server
                        memcpy(msg.payload, &subs, SUBSPACKET_LEN);
                        ret = send(sockfd, &msg, MSG_LEN, 0);
                        DIE(ret < 0, "send unsubscribe");

                        // Receiv feedback from server to confirm unsubscription
                        memset(buffer, 0, PACKET_LEN);
                        ret = recv(sockfd, buffer, MSG_LEN, 0);
                        DIE(ret < 0, "recv feedback");

                        // Print command feedback
                        struct proto_msg *feedback = (struct proto_msg *)buffer;
                        if (feedback->msg_type == CONF_MSG) {
                            std::cout << "Unsubscribed from topic.\n";
                        } else if (debug) {
                            std::cerr << "Unexpected message type received!\n";
                        }
                    } else if (debug) {
                        std::cerr << "Invalid command!\n";
                    }
                }
            }
        }
    }

    // Close TCP server socket
    close(sockfd);

    return 0;
}
