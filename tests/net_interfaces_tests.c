#include <stdio.h>
#include <string.h>

#include "net_interfaces.h"

#define CHECK(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #expression); \
		return 1; \
	} \
} while (0)

int main(void)
{
	net_interface_info_t interfaces[3];
	net_interface_info_t live[64];
	size_t live_count, i;
	char index_selector[32];

	memset(interfaces, 0, sizeof(interfaces));
	snprintf(interfaces[0].name, sizeof(interfaces[0].name), "Ethernet");
	snprintf(interfaces[0].address, sizeof(interfaces[0].address), "192.168.0.214");
	interfaces[0].index = 22;
	snprintf(interfaces[1].name, sizeof(interfaces[1].name), "Wi-Fi");
	snprintf(interfaces[1].address, sizeof(interfaces[1].address), "192.168.1.15");
	interfaces[1].index = 7;
	snprintf(interfaces[2].name, sizeof(interfaces[2].name), "sing-tun");
	snprintf(interfaces[2].address, sizeof(interfaces[2].address), "172.19.0.1");
	interfaces[2].index = 44;

	CHECK(NETIF_Find(interfaces, 3, "auto") == -1);
	CHECK(NETIF_Find(interfaces, 3, "AUTO") == -1);
	CHECK(NETIF_Find(interfaces, 3, "") == -1);
	CHECK(NETIF_Find(interfaces, 3, "ethernet") == 0);
	CHECK(NETIF_Find(interfaces, 3, "192.168.1.15") == 1);
	CHECK(NETIF_Find(interfaces, 3, "44") == 2);
	CHECK(NETIF_Find(interfaces, 3, "index:22") == 0);
	CHECK(NETIF_Find(interfaces, 3, "missing") == -1);

	/* Smoke-test the platform enumerator and every selector form it returns. */
	live_count = NETIF_Enumerate(live, sizeof(live) / sizeof(live[0]));
	for (i = 0; i < live_count; ++i) {
		CHECK(live[i].name[0] != '\0');
		CHECK(live[i].address[0] != '\0');
		CHECK(live[i].index != 0);
		CHECK(NETIF_Find(live, live_count, live[i].name) == (int)i);
		CHECK(NETIF_Find(live, live_count, live[i].address) == (int)i);
		snprintf(index_selector, sizeof(index_selector), "index:%u", live[i].index);
		CHECK(NETIF_Find(live, live_count, index_selector) == (int)i);
	}
	puts("net interface selector tests passed");
	return 0;
}
