#ifndef EZQUAKE_NET_INTERFACES_H
#define EZQUAKE_NET_INTERFACES_H

#include <stddef.h>

#define NET_INTERFACE_NAME_SIZE 128
#define NET_INTERFACE_ADDRESS_SIZE 16

typedef struct net_interface_info_s {
	char name[NET_INTERFACE_NAME_SIZE];
	char address[NET_INTERFACE_ADDRESS_SIZE];
	unsigned int index;
	int has_gateway;
} net_interface_info_t;

/* Returns active, non-loopback IPv4 interfaces, de-duplicated by name. */
size_t NETIF_Enumerate(net_interface_info_t *interfaces, size_t capacity);

/* Accepts an interface name, IPv4 address, decimal index, or "index:<n>". */
int NETIF_Find(const net_interface_info_t *interfaces, size_t count, const char *selector);

#endif
