#include <queue.h>
#include "skel.h"

// Routing Table Entry Struct
typedef struct rtable_entry {
	uint32_t prefix;
	uint32_t next_hop;
	uint32_t mask;
	int interface;
} __attribute__((packed)) rtable_entry;

// ARP Table Entry Struct
typedef struct arptable_entry {
	uint32_t ip;
	uint8_t mac[6];
} __attribute__((packed)) arptable_entry;

// Bucket Struct used for holding all rtable entries with a certain mask
typedef struct bucket {
	rtable_entry **entries;
	int size;
	int capacity;
} bucket;

// Routing Table Struct used to store the entire routing table
typedef struct rtable {
	bucket **buckets;
} rtable;

// ARP Table Struct used for storing all ARP entries
typedef struct arptable {
	arptable_entry **entries;
	int size;
	int capacity;
} arptable;

// Allocate space and initialise for routing table
rtable* init_rtable() {
	rtable *my_rtable = (rtable*)malloc(sizeof(rtable));
	// Allocate 33 buckets for prefix lengths from /0 to /32
	my_rtable->buckets = (bucket**)malloc(33 * sizeof(bucket*));
	for (int i = 0; i < 33; i++) {
		my_rtable->buckets[i] = (bucket*)malloc(sizeof(bucket));
		bucket *bucket = my_rtable->buckets[i];
		bucket->size = 0;
		bucket->capacity = 1;
		// Allocate space for entries in buckets
		bucket->entries = (rtable_entry**)malloc(bucket->capacity * 
														sizeof(rtable_entry*));
	}
	return my_rtable;
}

// Allocate space and initialise ARP table
arptable* init_arptable() {
	arptable* my_arptable = (arptable*)malloc(sizeof(arptable));
	my_arptable->size = 0;
	my_arptable->capacity = 1;
	// Allocate space for entries
	my_arptable->entries = (arptable_entry**)malloc(my_arptable->capacity * 
													sizeof(arptable_entry*));
	return my_arptable;
}

// Free allocated space for given rtable
void free_rtable(rtable *my_rtable) {
	for (int i = 0; i < 33; i++) {
		bucket *bucket = my_rtable->buckets[i];
		for (int j = 0; j < bucket->size; j++) {
			free(bucket->entries[j]);
		}
		free(bucket->entries);
		free(bucket);
	}
	free(my_rtable->buckets);
	free(my_rtable);
}

// Free allocated space for given arptable
void free_arptable(arptable *my_arptable) {
	for (int i = 0; i < my_arptable->size; i++) {
		free(my_arptable->entries[i]);
	}
	free(my_arptable->entries);
	free(my_arptable);
}

// Add IP and MAC as entry in arptable
void add_to_arptable(arptable *my_arptable, uint32_t ip, uint8_t *mac) {
	// Allocate space for new entry
	arptable_entry *entry = (arptable_entry*)malloc(sizeof(arptable_entry));
	entry->ip = ip;
	memcpy(entry->mac, mac, ETH_ALEN);
	// Add entry at the end of ARP table
	my_arptable->entries[my_arptable->size] = entry;
	my_arptable->size++;
	// If arptable fills up to capacity, double capacity and reallocate
	if (my_arptable->size == my_arptable->capacity) {
		my_arptable->capacity *= 2;
		my_arptable->entries = (arptable_entry**)realloc(my_arptable->entries,
													my_arptable->capacity * 
													sizeof(arptable_entry*));
	}
}

// Get arp table entry based on given IP
arptable_entry* get_mac(arptable *my_arptable, uint32_t ip) {
	for (int i = 0; i < my_arptable->size; i++) {
		if (my_arptable->entries[i]->ip == ip) {
			return my_arptable->entries[i];
		}
	}
	// Return null if no entry was found
	return NULL;
}

// Compare two routing table entries based on prefixes to sort them
int entry_compare(const void *a, const void *b) {
	rtable_entry *entry_a = *(rtable_entry**)a;
	rtable_entry *entry_b = *(rtable_entry**)b;
	return (entry_a->prefix - entry_b->prefix);
}

// Calculate prefix length from given subnet mask
int get_prefix_length(uint32_t ip_addr) {
    uint32_t ip = ntohl(ip_addr);
    int prefix = 0;
    while (ip > 0) {
        ip = ip << 1;
        prefix++;
    }
    return prefix;
}

// Calculate subnet mask from given prefix length
uint32_t get_subnet_mask(int prefix) {
    uint32_t ip = ((1 << prefix) - (prefix == 32 ? 2 : 1)) << (32 - prefix);
    ip = htonl(ip);
    return ip;
}

// Parse routing table from given filename
void parse_rtable(rtable *my_rtable, char *rtable_file) {
	FILE *f = fopen(rtable_file, "r");
	DIE(f == NULL, "rtable file open");

	char prefix[16];
	char next_hop[16];
	char mask[16];
	int interface;

	while (!feof(f)) {
		fscanf(f, "%s %s %s %d\n", prefix, next_hop, mask, &interface);

		// Allocate space for a new entry and read data into it
		rtable_entry *entry = (rtable_entry*)malloc(sizeof(rtable_entry));
		entry->prefix = inet_addr(prefix);
		entry->next_hop = inet_addr(next_hop);
		entry->mask = inet_addr(mask);
		entry->interface = interface;

		// Calculate bucket in which to be placed based on subnet mask
		int idx = get_prefix_length(entry->mask);
		bucket *bucket = my_rtable->buckets[idx];

		bucket->entries[bucket->size] = entry;
		bucket->size++;
		if (bucket->size == bucket->capacity) {
			bucket->capacity *= 2;
			bucket->entries = (rtable_entry**)realloc(bucket->entries,
														bucket->capacity * 
														sizeof(rtable_entry*));
		}
	}

	// After file has been parsed, sort buckets with entries based on prefix
	for (int i = 0; i < 33; i++) {
		bucket *bucket = my_rtable->buckets[i];
		if (bucket->size != 0) {
			qsort(bucket->entries, bucket->size,
					sizeof(rtable_entry*),entry_compare);
		}
	}

	fclose(f);
}

// Get the best route from the routing table for a given destiation IP
rtable_entry* get_best_route(rtable *my_rtable, uint32_t dest_ip) {
	// Iterate through buckets from largest mask to lowest to get LMP
	for (int i = 32; i >= 0; i--) {
		bucket *bucket = my_rtable->buckets[i];
		uint32_t s_mask = get_subnet_mask(i);
		// Check if bucket contains entries
		if (bucket->size != 0) {
			// Perform binary search on bucket to find matching prefix
			int left = 0;
			int right = bucket->size - 1;
			while (left <= right) {
				int mid = (left + right) / 2;
				int to_match = (dest_ip & s_mask);
				if (to_match == bucket->entries[mid]->prefix) {
					return bucket->entries[mid];
				}
				if (to_match < bucket->entries[mid]->prefix) {
					right = mid - 1;
				}
				if (to_match > bucket->entries[mid]->prefix) {
					left = mid + 1;
				}
			}
		}
	}
	// Return null if no route was found
	return NULL;
}

int main(int argc, char *argv[]) {
	packet m;
	int rc;

	init(argc - 2, argv + 2);

	// Create Routing and ARP tables and packet queue
	rtable *my_rtable = init_rtable();
	arptable *my_arptable = init_arptable();
	queue q;
	q = queue_create();
	parse_rtable(my_rtable, argv[1]);

	// Start router mechanism
	while (1) {
		// Receive packet from interfaces
		rc = get_packet(&m);
		DIE(rc < 0, "get_message");
		// Extract ethernet header from packet
		struct ether_header *eth_hdr = (struct ether_header*)m.payload;
		// Extract IP header from packet (not existent if packet is ARP packet)
		struct iphdr *ip_hdr = (struct iphdr*)(m.payload + 
												sizeof(struct ether_header));
		// Extract ICMP header from packet if packet is an ICMP packet
		struct icmphdr *icmp_hdr = parse_icmp(m.payload);
		// Extract ARP header from packet if packet is an ARP packet
		struct arp_header *arp_hdr = parse_arp(m.payload);
		// If packet is ICMP
		if (icmp_hdr != NULL) {
			// Respong to ICMP ECHO REQUEST if targeted at router
			if (icmp_hdr->type == ICMP_ECHO &&
				ip_hdr->daddr == inet_addr(get_interface_ip(m.interface))) {
				send_icmp(ip_hdr->saddr, ip_hdr->daddr, eth_hdr->ether_dhost,
							eth_hdr->ether_shost,ICMP_ECHOREPLY, 0, m.interface,
							icmp_hdr->un.echo.id, icmp_hdr->un.echo.sequence);
				continue;
			}
		}
		// If packet is ICMP
		if (arp_hdr != NULL) {
			// Respond to ARP REQUEST if targeted at router
			if (arp_hdr->op == htons(ARPOP_REQUEST) &&
				arp_hdr->tpa == inet_addr(get_interface_ip(m.interface))) {
				// Build ether header for ARP REPLY
				struct ether_header new_eth_hdr;
				uint8_t mac[ETH_ALEN];
				get_interface_mac(m.interface, mac);
				build_ethhdr(&new_eth_hdr, mac,
							arp_hdr->sha, htons(ETHERTYPE_ARP));
				// Send ARP REPLY
				send_arp(arp_hdr->spa, arp_hdr->tpa, &new_eth_hdr,
						m.interface, htons(ARPOP_REPLY));
				continue;
			}
			// Update arp table with data from ARP REPLY packet
			if (arp_hdr->op == htons(ARPOP_REPLY)) {
				add_to_arptable(my_arptable, arp_hdr->spa, arp_hdr->sha);
				// Check if there are packets queued for transmission
				if (queue_empty(q)) {
					continue;
				}
				// Get first packet from queue
				packet *p = (packet*)queue_top(q);
				struct ether_header *p_eth_hdr;
				p_eth_hdr = (struct ether_header*)p->payload;
				struct iphdr *p_ip_hdr;
				p_ip_hdr = (struct iphdr*)(p->payload +
											sizeof(struct ether_header));
				rtable_entry *best_route = get_best_route(my_rtable,
															p_ip_hdr->daddr);
				// Check if ARP REPLY was received from packet next-hop
				while (best_route->next_hop == arp_hdr->spa) {
					// Remove packet from queue
					p = (packet*)queue_deq(q);
					// Update packet destination mac and send 
					memcpy(p_eth_hdr->ether_dhost, arp_hdr->sha, ETH_ALEN);
					send_packet(best_route->interface, p);
					free(p);
					// Check if there are more packets in queue
					if (queue_empty(q)) {
						break;
					}
					// Get next packet from queue
					p = (packet*)queue_top(q);
					best_route = get_best_route(my_rtable, p_ip_hdr->daddr);
				}
				continue;
			}
		}
		// Send TIMEOUT message if packet TTL has reached 1
		if (ip_hdr->ttl <= 1) {
			send_icmp_error(ip_hdr->saddr, ip_hdr->daddr, eth_hdr->ether_dhost,
							eth_hdr->ether_shost, ICMP_TIME_EXCEEDED,
							0, m.interface);
			continue;
		}
		// Drop packet if checksum is incorrect
		if (ip_checksum(ip_hdr, sizeof(struct iphdr)) != 0) {
			continue;
		}
		// Decrement TTL and update checksum
		ip_hdr->ttl--;
		ip_hdr->check = 0;
		ip_hdr->check = ip_checksum(ip_hdr, sizeof(struct iphdr));
		// Try to get route for packet from routing table
		rtable_entry *best_route = get_best_route(my_rtable, ip_hdr->daddr);
		// If no route was found send DESTINATION UNREACHABLE message
		if (best_route == NULL) {
			send_icmp_error(ip_hdr->saddr, ip_hdr->daddr, eth_hdr->ether_dhost,
							eth_hdr->ether_shost, ICMP_DEST_UNREACH,
							0, m.interface);
			continue;
		}
		// Update source MAC address from route interface
		get_interface_mac(best_route->interface, eth_hdr->ether_shost);
		// Try to get mac for route next-hop IP from ARp table
		arptable_entry *arp_entry = get_mac(my_arptable, best_route->next_hop);
		// If no entry has been found queue the packet and perform ARP REQUEST
		if (arp_entry == NULL) {
			// Copy packet to memory and store in queue
			packet *p = (packet*)malloc(sizeof(packet));
			memcpy(p, &m, sizeof(m));
			queue_enq(q, p);
			// Build ether header for ARP REQUEST
			struct ether_header new_eth_hdr;
			uint8_t smac[ETH_ALEN];
			uint8_t dmac[ETH_ALEN];
			// Use broadcast MAC address as destination address
			hwaddr_aton("ff:ff:ff:ff:ff:ff", dmac);
			int intf = best_route->interface;
			get_interface_mac(intf, smac);
			build_ethhdr(&new_eth_hdr, smac, dmac, htons(ETHERTYPE_ARP));
			// Send ARP REQUEST for packet next-hop via packet route interface
			send_arp(best_route->next_hop, inet_addr(get_interface_ip(intf)),
					&new_eth_hdr, intf, htons(ARPOP_REQUEST));
			continue;
		}
		// Update destination MAC from ARP table and send via route interface
		memcpy(eth_hdr->ether_dhost, arp_entry->mac, ETH_ALEN);
		send_packet(best_route->interface, &m);
	}

	// Free all allocated space
	free(q);
	free_rtable(my_rtable);
	free_arptable(my_arptable);
	return 0;
}
