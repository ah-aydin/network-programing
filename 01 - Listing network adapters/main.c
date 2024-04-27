#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

int main() {
  struct ifaddrs *addresses;
  if (getifaddrs(&addresses) == -1) {
    printf("getifaddrs call failed\n");
    return -1;
  }

  struct ifaddrs *address = addresses;
  while (address) {
    int family = address->ifa_addr->sa_family;
    if (family == AF_INET || family == AF_INET6) {
      printf("%s (%s)\n", address->ifa_name,
             family == AF_INET ? "IPv4" : "IPv6");

      char host[128];
      const int family_size = family == AF_INET ? sizeof(struct sockaddr_in)
                                                : sizeof(struct sockaddr_in6);
      getnameinfo(address->ifa_addr, family_size, host, sizeof(host), NULL, 0,
                  NI_NUMERICHOST);
      printf("\t%s\n", host);
    }
    address = address->ifa_next;
  }

  freeifaddrs(addresses);
  return 0;
}
