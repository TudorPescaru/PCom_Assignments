// Copyright 2021 Pescaru Tudor-Mihai 321CA
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <iostream>
#include <set>
#include <unordered_map>
#include <string>
#include <queue>

#include "./protocol.h"

// Class which defines a client and stores all of it's attributes
class Client {
 private:
    std::string id;  // Client id
    bool connected;  // Client connection status
    int fd;  // Client current socket fd
    std::unordered_map<std::string, bool> topics;  // Map of topics and sf value
    std::queue<struct packet*> sf_queue;  // Queue of packets to be sent

 public:
    Client(int fd, std::string id) {
        // Set fd, id and connected state
        this->fd = fd;
        this->id = id;
        connected = true;
    }

    ~Client() {
        // Free memory for any undelivered packets
        while (!sf_queue.empty()) {
            struct packet *pkt = sf_queue.front();
            sf_queue.pop();
            delete pkt;
        }
    }

    // Return fd of a client
    int get_fd() {
        return fd;
    }

    // Set connected state to false to disconnect client
    void disconnect() {
        connected = false;
    }

    // Reconnect client and send all messages received while disconnected
    void reconnect(int fd) {
        struct proto_msg msg;
        struct size_packet sz;
        char buffer[PACKET_LEN];
        // Update fd of socket connection
        this->fd = fd;
        // Set connected state to true
        connected = true;
        // If there are packets queued up send them
        while (!sf_queue.empty()) {
            // Get first packet in queue
            struct packet *pkt = sf_queue.front();
            sf_queue.pop();

            // Create an incoming packet protocol message
            memset(&msg, 0, MSG_LEN);
            msg.msg_type = PKT_MSG;

            // Populate sizes struct with sizes of packet components
            // and total packet size
            sz.topic_size = strlen(pkt->udp_msg.topic) < TOPIC_LEN ?
                            strlen(pkt->udp_msg.topic) + 1 : TOPIC_LEN;

            // Set content size based on data type
            if (pkt->udp_msg.data_type == 0) {
                sz.content_size = sizeof(char) + sizeof(uint32_t);
            } else if (pkt->udp_msg.data_type == 1) {
                sz.content_size = sizeof(uint16_t);
            } else if (pkt->udp_msg.data_type == 2) {
                sz.content_size = sizeof(char) + sizeof(uint32_t)
                                    + sizeof(uint8_t);
            } else if (pkt->udp_msg.data_type == 3) {
                sz.content_size = strlen(pkt->udp_msg.content) < CONTENT_LEN ?
                                strlen(pkt->udp_msg.content) + 1 : CONTENT_LEN;
            }

            // Load packet components into buffer without padding
            sz.to_receive = 0;
            memset(buffer, 0, PACKET_LEN);
            memcpy(buffer + sz.to_receive, &(pkt->udp_client_ip),
                    sizeof(struct in_addr));
            sz.to_receive += sizeof(struct in_addr);
            memcpy(buffer + sz.to_receive, &(pkt->udp_client_port),
                    sizeof(uint16_t));
            sz.to_receive += sizeof(uint16_t);
            memcpy(buffer + sz.to_receive, pkt->udp_msg.topic, sz.topic_size);
            sz.to_receive += sz.topic_size;
            memcpy(buffer + sz.to_receive, &(pkt->udp_msg.data_type),
                    sizeof(uint8_t));
            sz.to_receive += sizeof(uint8_t);
            memcpy(buffer + sz.to_receive, pkt->udp_msg.content,
                    sz.content_size);
            sz.to_receive += sz.content_size;

            // Load sizes struct into protocol message payload and send message
            memcpy(msg.payload, &sz, SIZEPACKET_LEN);
            int ret = send(this->fd, &msg, MSG_LEN, 0);
            DIE(ret < 0, "send message");

            // Send buffer
            ret = send(this->fd, buffer, sz.to_receive, 0);
            DIE(ret < 0, "send packet");

            // Free memory used to store packet
            delete pkt;
        }
    }

    // Return connected state
    bool is_connected() {
        return connected;
    }

    // Subscribe to given topic with given sf value
    void subscribe(std::string topic, bool sf) {
        topics.insert_or_assign(topic, sf);
    }

    // Unsubscribe from given topic
    void unsubscribe(std::string topic) {
        topics.erase(topic);
    }

    // Check if given topic has sf turned on
    bool topic_has_sf(std::string topic) {
        return topics[topic];
    }

    // Store given packet in the queue if client is disconnected
    void store_packet(struct packet pkt) {
        struct packet *to_add = new packet();
        memset(to_add, 0, PACKET_LEN);
        memcpy(to_add, &pkt, PACKET_LEN);
        sf_queue.push(to_add);
    }

    // Send given packet
    void send_packet(struct packet pkt) {
        struct proto_msg msg;
        struct size_packet sz;
        char buffer[PACKET_LEN];

        // Create an incoming packet protocol message
        memset(&msg, 0, MSG_LEN);
        msg.msg_type = PKT_MSG;

        // Populate sizes struct with sizes of packet components
        // and total packet size
        sz.topic_size = strlen(pkt.udp_msg.topic) < TOPIC_LEN ?
                        strlen(pkt.udp_msg.topic) + 1 : TOPIC_LEN;

        // Set content size based on data type
        if (pkt.udp_msg.data_type == 0) {
            sz.content_size = sizeof(char) + sizeof(uint32_t);
        } else if (pkt.udp_msg.data_type == 1) {
            sz.content_size = sizeof(uint16_t);
        } else if (pkt.udp_msg.data_type == 2) {
            sz.content_size = sizeof(char) + sizeof(uint32_t) + sizeof(uint8_t);
        } else if (pkt.udp_msg.data_type == 3) {
            sz.content_size = strlen(pkt.udp_msg.content) < CONTENT_LEN ?
                                strlen(pkt.udp_msg.content) + 1 : CONTENT_LEN;
        }

        // Load packet components into buffer without padding
        sz.to_receive = 0;
        memset(buffer, 0, PACKET_LEN);
        memcpy(buffer + sz.to_receive, &(pkt.udp_client_ip),
                sizeof(struct in_addr));
        sz.to_receive += sizeof(struct in_addr);
        memcpy(buffer + sz.to_receive, &(pkt.udp_client_port),
                sizeof(uint16_t));
        sz.to_receive += sizeof(uint16_t);
        memcpy(buffer + sz.to_receive, pkt.udp_msg.topic, sz.topic_size);
        sz.to_receive += sz.topic_size;
        memcpy(buffer + sz.to_receive, &(pkt.udp_msg.data_type),
                sizeof(uint8_t));
        sz.to_receive += sizeof(uint8_t);
        memcpy(buffer + sz.to_receive, pkt.udp_msg.content, sz.content_size);
        sz.to_receive += sz.content_size;

        // Load sizes struct into protocol message payload and send message
        memcpy(msg.payload, &sz, SIZEPACKET_LEN);
        int ret = send(this->fd, &msg, MSG_LEN, 0);
        DIE(ret < 0, "send message");

        // Send buffer
        ret = send(this->fd, buffer, sz.to_receive, 0);
        DIE(ret < 0, "send packet");
    }
};

// Send exit protocol message to given socket fd
void send_exit(int fd) {
    struct proto_msg msg;

    memset(&msg, 0, MSG_LEN);
    msg.msg_type = EXIT_MSG;

    int ret = send(fd, &msg, MSG_LEN, 0);
    DIE(ret < 0, "send exit signals");
}

int main(int argc, char **argv) {
    // Disable STDOUT buffering
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    // Enable or disable debugging mode messages
    bool debug = true;

    // Check if correct number of args was given
    if (argc != 2) {
        if (debug) {
            std::cerr << "Usage is ./server <SERVER_PORT>\n";
        }
        exit(0);
    }

    int udpsockfd, tcpsockfd, portno, ret, fdmax, i, newsockfd;
    char buffer[PACKET_LEN];
    bool exit_status = false;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t cli_len;
    fd_set read_fds, tmp_fds;

    // Data structure used to map client ids to client objects
    std::unordered_map<std::string, Client*> clients;
    // Data structure used to map socket fds to client ids
    std::unordered_map<int, std::string> fd_to_id;
    // Data structure used to map topics to clients subscribed to them
    std::unordered_map<std::string, std::set<Client*>> topic_to_clients;
    // Set of client object references used for memory deallocation
    std::set<Client*> client_refs;

    // Initialize file descriptor sets
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    // Create UDP socket for fommunication with UDP clients
    udpsockfd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udpsockfd < 0, "open UDP socket");

    // Create TCP socket to listen to TCP client connections
    tcpsockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcpsockfd < 0, "open TCP socket");

    // Disable Neagle's algorithm
    int optval = 1;
    ret = setsockopt(tcpsockfd, IPPROTO_TCP, TCP_NODELAY,
                    reinterpret_cast<void *>(&optval), sizeof(optval));
    DIE(ret < 0, "set TCP_NODELAY");
    // Enable port reusage for UDP and TCP
    ret = setsockopt(udpsockfd, SOL_SOCKET, SO_REUSEADDR,
                    reinterpret_cast<void *>(&optval), sizeof(optval));
    DIE(ret < 0, "set SO_REUSEADDR for UDP");
    ret = setsockopt(tcpsockfd, SOL_SOCKET, SO_REUSEADDR,
                    reinterpret_cast<void *>(&optval), sizeof(optval));
    DIE(ret < 0, "set SO_REUSEADDR for TCP");

    // Convert given port number to int
    portno = atoi(argv[1]);
    DIE(portno == 0, "port atoi");

    // Set IP and port to connect to server
    memset(reinterpret_cast<char *>(&serv_addr), 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    serv_addr.sin_family = INADDR_ANY;

    // Bind UDP socket fd to addr structure
    ret = bind(udpsockfd, (struct sockaddr *) &serv_addr,
                sizeof(struct sockaddr));
    DIE(ret < 0, "bind UDP socket");

    // Bind UDP socket fd to addr structure
    ret = bind(tcpsockfd, (struct sockaddr *) &serv_addr,
                sizeof(struct sockaddr));
    DIE(ret < 0, "bind TCP socket");

    // Listen for TCP client connections
    ret = listen(tcpsockfd, MAX_CLIENTS);
    DIE(ret < 0, "listen to clients on TCP socket");

    // Add STDIN, UDP socket fd and TCP socket fd to fd set
    FD_SET(STDIN_FILENO, &read_fds);
    FD_SET(udpsockfd, &read_fds);
    FD_SET(tcpsockfd, &read_fds);
    fdmax = tcpsockfd;

    while (!exit_status) {
        tmp_fds = read_fds;

        // Select socket fds to read from
        ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(ret < 0, "select");

        for (i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &tmp_fds)) {
                if (i == STDIN_FILENO) {
                    // Read command from STDIN
                    memset(buffer, 0, PACKET_LEN);
                    std::cin >> buffer;

                    // If command is exit set exit state and send exit messages
                    // to all connected clients
                    if (strncmp(buffer, "exit", EXIT_LEN) == 0) {
                        for (int j = 3; j <= fdmax; j++) {
                            if (FD_ISSET(j, &read_fds)
                                && j != udpsockfd && j != tcpsockfd) {
                                send_exit(j);
                                close(j);
                                FD_CLR(j, &read_fds);
                            }
                        }
                        exit_status = true;
                        break;
                    } else if (debug) {
                        std::cerr << "Invalid command!\n";
                    }
                } else if (i == udpsockfd) {
                    // Receive packets from UDP clients
                    memset(buffer, 0, PACKET_LEN);
                    cli_len = sizeof(cli_addr);
                    ret = recvfrom(i, buffer, UDPPACKET_LEN, 0,
                                    (struct sockaddr *) &cli_addr, &cli_len);
                    DIE(ret < 0, "recv from UDP clients");

                    // Load received buffer into packet struct
                    struct packet pkt;
                    memset(&pkt, 0, PACKET_LEN);
                    memcpy(&pkt.udp_client_ip, &cli_addr.sin_addr,
                            sizeof(struct in_addr));
                    pkt.udp_client_port = cli_addr.sin_port;
                    memset(&pkt.udp_msg, 0, UDPPACKET_LEN);
                    memcpy(&pkt.udp_msg, buffer, UDPPACKET_LEN);

                    // Get list of clients subscribed to topic of packet
                    std::string topic = pkt.udp_msg.topic;
                    for (Client* cl : topic_to_clients[topic]) {
                        // If client is connected send else queue the packet
                        if (cl->is_connected()) {
                            cl->send_packet(pkt);
                        } else if (cl->topic_has_sf(topic)) {
                            cl->store_packet(pkt);
                        }
                    }
                } else if (i == tcpsockfd) {
                    // Accept connection from TCP client
                    cli_len = sizeof(cli_addr);
                    newsockfd = accept(tcpsockfd, (struct sockaddr *) &cli_addr,
                                        &cli_len);
                    DIE(newsockfd < 0, "accept TCP client");

                    // Add client socket fd to set
                    FD_SET(newsockfd, &read_fds);
                    if (newsockfd > fdmax) {
                        fdmax = newsockfd;
                    }

                    // Receive client ID of newly connected client
                    memset(buffer, 0, PACKET_LEN);
                    ret = recv(newsockfd, buffer, MSG_LEN, 0);
                    DIE(ret < 0, "recv from UDP clients");

                    // Get client ID from received message
                    struct proto_msg *msg = (struct proto_msg *)buffer;
                    if (msg->msg_type == ID_MSG) {
                        std::string cli_id = msg->payload;
                        // Check if client ID has already been used
                        if (clients.find(cli_id) != clients.end()) {
                            // Check if client with client ID is connected
                            if (clients[cli_id]->is_connected()) {
                                // If already connected send exit to new client
                                // and close new connection
                                std::cout << "Client " << cli_id
                                            << " already connected.\n";
                                send_exit(newsockfd);
                                close(newsockfd);
                                FD_CLR(newsockfd, &read_fds);
                                continue;
                            } else {
                                // If client not connected reconnect client
                                // and update fd
                                Client *cl = clients[cli_id];
                                int old_fd = cl->get_fd();
                                cl->reconnect(newsockfd);
                                fd_to_id.erase(old_fd);
                                fd_to_id.insert_or_assign(newsockfd, cli_id);
                            }
                        } else {
                            // If ID is new create new client object and store
                            Client *cl = new Client(newsockfd, cli_id);
                            clients.insert({cli_id, cl});
                            fd_to_id.insert_or_assign(newsockfd, cli_id);
                            client_refs.insert(cl);
                        }
                        // Print new connection message
                        std::cout << "New client " << cli_id
                                    << " connected from "
                                    << inet_ntoa(cli_addr.sin_addr) << ":"
                                    << ntohs(cli_addr.sin_port) << ".\n";
                    } else if (debug) {
                        std::cerr << "Unexpected message type received!\n";
                    }
                } else {
                    // Receive message from TCP clients
                    memset(buffer, 0, PACKET_LEN);
                    ret = recv(i, buffer, MSG_LEN, 0);
                    DIE(ret < 0, "recv from TCP clients");

                    // Check if connection has been closed
                    if (ret == 0) {
                        // Get client id for closed socket fd
                        std::string cli_id = fd_to_id[i];
                        // Set client connection status
                        Client *cl = clients[cli_id];
                        cl->disconnect();
                        // Print disconnect message
                        std::cout << "Client " << cli_id << " disconnected.\n";
                        // Close socket fd and remove from set
                        close(i);
                        FD_CLR(i, &read_fds);
                    } else {
                        // Convert received buffer to protocol message
                        struct proto_msg *msg = (struct proto_msg *)buffer;
                        // Convert protocol message payload to subs struct
                        struct subs_packet *subs;
                        subs = (struct subs_packet *)msg->payload;
                        // Check if message was a subscribe or unsubscribe msg
                        if (msg->msg_type == SUB_MSG) {
                            // Get client id and client object of socket fd
                            std::string cli_id = fd_to_id[i];
                            Client *cl = clients[cli_id];

                            // Subscribe client to received topic
                            cl->subscribe(subs->topic, subs->sf);
                            // Map client to topic
                            topic_to_clients[subs->topic].insert(cl);

                            // Send subscription confirm message to client
                            struct proto_msg feedback;
                            memset(&feedback, 0, MSG_LEN);
                            feedback.msg_type = CONF_MSG;

                            ret = send(i, &feedback, MSG_LEN, 0);
                            DIE(ret < 0, "send feedback");
                        } else if (msg->msg_type == UNSUB_MSG) {
                            // Get client id and client object of socket fd
                            std::string cli_id = fd_to_id[i];
                            Client *cl = clients[cli_id];

                            // Unsubscribe client to received topic
                            cl->unsubscribe(subs->topic);
                            // Erase client to topic mapping
                            topic_to_clients[subs->topic].erase(cl);

                            // Send unsubscription confirm message to client
                            struct proto_msg feedback;
                            memset(&feedback, 0, MSG_LEN);
                            feedback.msg_type = CONF_MSG;

                            ret = send(i, &feedback, MSG_LEN, 0);
                            DIE(ret < 0, "send feedback");
                        } else if (debug) {
                            std::cerr << "Unexpected message type received!\n";
                        }
                    }
                }
            }
        }
    }

    // Deallocate memory for client objects
    for (Client *cl : client_refs) {
        delete cl;
    }

    // Close UDP and TCP sockets
    close(udpsockfd);
    close(tcpsockfd);

    return 0;
}
