#include <arpa/inet.h>
#include <bits/types/struct_timeval.h>
#include <bits/types/timer_t.h>
#include <ctype.h>
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
  printf("Configuring local address...\n");
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  struct addrinfo *bind_address;
  if (getaddrinfo(0, "8080", &hints, &bind_address)) {
    eprintf("getaddrinfo() failed. (%d)\n", errno);
    return 1;
  }

  printf("Creating socket...\n");
  SOCKET socket_listen;
  socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype,
                         bind_address->ai_protocol);
  if (socket_listen == 0) {
    eprintf("socket() failed. (%d)\n", errno);
    freeaddrinfo(bind_address);
    return 1;
  }

  printf("Binding socket to local address...\n");
  if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen)) {
    eprintf("bind() failed. (%d)\n", errno);
    freeaddrinfo(bind_address);
    return 1;
  }
  freeaddrinfo(bind_address);

  fd_set master;
  FD_ZERO(&master);
  FD_SET(socket_listen, &master);
  SOCKET max_socket = socket_listen;
  printf("Waiting for connections...\n");

  while (1) {
    fd_set reads;
    reads = master;
    if (select(max_socket + 1, &reads, NULL, NULL, NULL) < 0) {
      eprintf("select() failed. (%d)\n", errno);
      return 1;
    }

    if (FD_ISSET(socket_listen, &reads)) {
      struct sockaddr_storage client_address;
      socklen_t client_len = sizeof(client_address);

      char buffer[4096];
      int bytes_received =
          recvfrom(socket_listen, buffer, sizeof(buffer), 0,
                   (struct sockaddr *)&client_address, &client_len);
      if (bytes_received < 1) {
        printf("Connection closed.\n");
        break;
      }

      for (int i = 0; i < bytes_received; ++i) {
        buffer[i] = toupper(buffer[i]);
      }
      sendto(socket_listen, buffer, bytes_received, 0,
             (struct sockaddr *)&client_address, client_len);
    }
  }

  printf("Closing socket...\n");
  close(socket_listen);
  printf("Finished.\n");

  return 0;
}
