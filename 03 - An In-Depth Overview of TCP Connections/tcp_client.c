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
#include <time.h>
#include <unistd.h>

#define SOCKET int
#define eprintf(format, ...) fprintf(stderr, format, __VA_ARGS__)

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "usage: tcp_client hostname port\n");
    return 1;
  }

  printf("Configuring remote address...\n");
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;

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
              NI_NUMERICHOST);
  printf(" %s %s\n", address_buffer, service_buffer);

  printf("Creating socket...\n");
  SOCKET socket_peer;
  socket_peer = socket(peer_address->ai_family, peer_address->ai_socktype,
                       peer_address->ai_protocol);
  if (socket < 0) {
    eprintf("socket() failed (%d)", errno);
    freeaddrinfo(peer_address);
    return 1;
  }

  printf("Connecting...\n");
  if (connect(socket_peer, peer_address->ai_addr, peer_address->ai_addrlen)) {
    eprintf("connect() failed (%d)", errno);
    freeaddrinfo(peer_address);
    return 1;
  }
  freeaddrinfo(peer_address);

  printf("Connected.\n");
  printf("To send data, enter text followed by enter\n");

  while (1) {
    fd_set reads;
    FD_ZERO(&reads);
    FD_SET(socket_peer, &reads);
    FD_SET(STDIN_FILENO, &reads);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;

    if (select(socket_peer + 1, &reads, NULL, NULL, &timeout) < 0) {
      eprintf("select() failed (%d)\n", errno);
      return 1;
    }

    if (FD_ISSET(socket_peer, &reads)) {
      char read_buffer[4096];
      int bytes_recieved =
          recv(socket_peer, read_buffer, sizeof(read_buffer), 0);
      if (bytes_recieved < 1) {
        printf("Connection closed by peer\n");
        break;
      }
      printf("Received (%d bytes): %.*s", bytes_recieved, bytes_recieved,
             read_buffer);
    }

    if (FD_ISSET(STDIN_FILENO, &reads)) {
      char read_buffer[4096];
      if (!fgets(read_buffer, sizeof(read_buffer), stdin)) {
        break;
      }
      printf("Sending: %s", read_buffer);
      int bytes_sent = send(socket_peer, read_buffer, strlen(read_buffer), 0);
      printf("Sent %d bytes.\n", bytes_sent);
    }
  }

  printf("Closing socket...\n");
  close(socket_peer);
  printf("Finished.\n");

  return 0;
}
