#include "net_interfaces.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

static int NETIF_StringEqual(const char *left, const char *right)
{
	while (*left && *right) {
		if (tolower((unsigned char)*left) != tolower((unsigned char)*right))
			return 0;
		++left;
		++right;
	}
	return *left == *right;
}

static int NETIF_IndexSelector(const char *selector, unsigned int *index)
{
	const char *number = selector;
	char *end = NULL;
	unsigned long parsed;

	if (!strncmp(selector, "index:", 6))
		number += 6;
	if (!*number)
		return 0;
	parsed = strtoul(number, &end, 10);
	if (!end || *end || parsed == 0 || parsed > 0xffffffffUL)
		return 0;
	*index = (unsigned int)parsed;
	return 1;
}

int NETIF_Find(const net_interface_info_t *interfaces, size_t count, const char *selector)
{
	size_t i;
	unsigned int index = 0;

	if (!interfaces || !selector || !*selector || NETIF_StringEqual(selector, "auto"))
		return -1;
	for (i = 0; i < count; ++i) {
		if (NETIF_StringEqual(selector, interfaces[i].name) ||
			!strcmp(selector, interfaces[i].address))
			return (int)i;
	}
	if (NETIF_IndexSelector(selector, &index)) {
		for (i = 0; i < count; ++i)
			if (interfaces[i].index == index)
				return (int)i;
	}
	return -1;
}

static int NETIF_AlreadyAdded(const net_interface_info_t *interfaces, size_t count, const char *name)
{
	size_t i;
	for (i = 0; i < count; ++i)
		if (NETIF_StringEqual(interfaces[i].name, name))
			return 1;
	return 0;
}

#ifdef _WIN32
static int NETIF_WideToUtf8(const wchar_t *wide, char *output, size_t output_size)
{
	int written;
	if (!wide || !output || output_size < 2)
		return 0;
	written = WideCharToMultiByte(CP_UTF8, 0, wide, -1, output, (int)output_size, NULL, NULL);
	if (!written) {
		output[0] = '\0';
		return 0;
	}
	return 1;
}

size_t NETIF_Enumerate(net_interface_info_t *interfaces, size_t capacity)
{
	IP_ADAPTER_ADDRESSES *adapters = NULL, *adapter;
	ULONG size = 16 * 1024;
	ULONG result;
	size_t count = 0;

	if (!interfaces || !capacity)
		return 0;
	for (;;) {
		adapters = (IP_ADAPTER_ADDRESSES *)malloc(size);
		if (!adapters)
			return 0;
		result = GetAdaptersAddresses(AF_INET,
			GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER |
			GAA_FLAG_INCLUDE_GATEWAYS,
			NULL, adapters, &size);
		if (result != ERROR_BUFFER_OVERFLOW)
			break;
		free(adapters);
		adapters = NULL;
	}
	if (result != NO_ERROR) {
		free(adapters);
		return 0;
	}

	for (adapter = adapters; adapter && count < capacity; adapter = adapter->Next) {
		IP_ADAPTER_UNICAST_ADDRESS *unicast;
		char name[NET_INTERFACE_NAME_SIZE];
		if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK ||
			!NETIF_WideToUtf8(adapter->FriendlyName, name, sizeof(name)) ||
			NETIF_AlreadyAdded(interfaces, count, name))
			continue;
		for (unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next) {
			struct sockaddr_in *address;
			if (!unicast->Address.lpSockaddr || unicast->Address.lpSockaddr->sa_family != AF_INET)
				continue;
			address = (struct sockaddr_in *)unicast->Address.lpSockaddr;
			if ((ntohl(address->sin_addr.s_addr) >> 24) == 127)
				continue;
			memset(&interfaces[count], 0, sizeof(interfaces[count]));
			snprintf(interfaces[count].name, sizeof(interfaces[count].name), "%s", name);
			if (!InetNtopA(AF_INET, &address->sin_addr, interfaces[count].address,
				(DWORD)sizeof(interfaces[count].address)))
				continue;
			interfaces[count].index = adapter->IfIndex;
			interfaces[count].has_gateway = adapter->FirstGatewayAddress != NULL;
			++count;
			break;
		}
	}
	free(adapters);
	return count;
}
#else
size_t NETIF_Enumerate(net_interface_info_t *interfaces, size_t capacity)
{
	struct ifaddrs *addresses = NULL, *entry;
	size_t count = 0;

	if (!interfaces || !capacity || getifaddrs(&addresses) != 0)
		return 0;
	for (entry = addresses; entry && count < capacity; entry = entry->ifa_next) {
		struct sockaddr_in *address;
		if (!entry->ifa_addr || entry->ifa_addr->sa_family != AF_INET ||
			!(entry->ifa_flags & IFF_UP) || (entry->ifa_flags & IFF_LOOPBACK) ||
			NETIF_AlreadyAdded(interfaces, count, entry->ifa_name))
			continue;
		address = (struct sockaddr_in *)entry->ifa_addr;
		memset(&interfaces[count], 0, sizeof(interfaces[count]));
		snprintf(interfaces[count].name, sizeof(interfaces[count].name), "%s", entry->ifa_name);
		if (!inet_ntop(AF_INET, &address->sin_addr, interfaces[count].address,
			sizeof(interfaces[count].address)))
			continue;
		interfaces[count].index = if_nametoindex(entry->ifa_name);
		/* Portable gateway discovery is unavailable here; do not label it missing. */
		interfaces[count].has_gateway = 1;
		++count;
	}
	freeifaddrs(addresses);
	return count;
}
#endif
