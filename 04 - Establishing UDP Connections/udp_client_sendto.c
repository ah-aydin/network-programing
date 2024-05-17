#include <arpa/inet.h>
#include <bits/types/struct_timeval.h>
#include <bits/types/timer_t.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define SOCKET int
#define eprintf(format, ...) fprintf(stderr, format, __VA_ARGS__)

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "usage: udp_client hostname port\n");
    return 1;
  }

  printf("Configuring remote address...\n");
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_DGRAM;

  struct addrinfo *peer_address;
  if (getaddrinfo(argv[1], argv[2], &hints, &peer_address)) {
    eprintf("getaddrinfo() failed (%d)\n", errno);
    return 1;
  }

  printf("Remote address is:");
  char address_buffer[128];
  char service_buffer[128];
  getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen, address_buffer,
              sizeof(address_buffer), service_buffer, sizeof(service_buffer),
              NI_NUMERICSERV | NI_NUMERICHOST);
  printf("%s %s\n", address_buffer, service_buffer);

  printf("Creating socket...\n");
  SOCKET socket_peer;
  socket_peer = socket(peer_address->ai_family, peer_address->ai_socktype,
                       peer_address->ai_protocol);
  if (socket_peer == 0) {
    eprintf("socket() failed. (%d)\n", errno);
    freeaddrinfo(peer_address);
    return 1;
  }

  char message[128];
  fgets(message, sizeof(message), stdin);
  printf("Sending: %s\n", message);
  int bytes_sent = sendto(socket_peer, message, strlen(message), 0,
                          peer_address->ai_addr, peer_address->ai_addrlen);
  printf("Sent %d bytes.\n", bytes_sent);

  freeaddrinfo(peer_address);
  printf("Closing socket...\n");
  close(socket_peer);
  printf("Finished.\n");

  return 0;
}
