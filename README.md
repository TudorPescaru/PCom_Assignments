# PCom_Assignments
Assignments for the Communication Protocols Course at the Faculty of Automatic 
Control and Computers from the "Politehnica" University of Bucharest. 

## Assignments

### PCom1

This assignment involved implementing a router with support for routing ICMP 
and ARP protocols between hosts and other routers. The router implementation 
features a full routing table parsing and route matching featureset as well 
as support for ARP table population and querying. A packet queue has also been 
implemented to store packets which cannot be forwarded currently and require 
an ARP Reply.

### PCom2

This assignment involve implementing a client-server application using TCP and 
UDP socket-based communication. Clients are split into two categories, TCP 
clients and UDP clients. UDP clients send messages to the server, each message 
containing a topic. The server's job is to forward the message to all TCP 
clients who are subscribed to the message's topic. The TCP clients can 
subscribe and unsubscribe from at any time from any topic. The server also 
features a store-and-forward policy which clients can enable for specific 
topics. When a client has store-and-forward enabled for a specific topic, all 
mesages the client was supposed to receive while disconnected are stored and 
sent upon reconnection.

### PCom3

This assignment involved creating a client that will interract with a REST 
API on a web server using a set of commands and GET, POST and DELETE requests. 
The API simulates an online book library and the reuqests and responses use 
JSON content. The Parson C library was used provide JSON functionality.

