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
  SOCKET socket_server;
  socket_server = socket(bind_address->ai_family, bind_address->ai_socktype,
                         bind_address->ai_protocol);
  if (socket_server == 0) {
    eprintf("socket() failed. (%d)\n", errno);
    freeaddrinfo(bind_address);
    return 1;
  }

  printf("Binding socket to local address...\n");
  if (bind(socket_server, bind_address->ai_addr, bind_address->ai_addrlen)) {
    eprintf("bind() failed. (%d)\n", errno);
    freeaddrinfo(bind_address);
    return 1;
  }
  freeaddrinfo(bind_address);

  struct sockaddr_storage client_address;
  socklen_t client_len = sizeof(client_address);
  char buffer[1024];
  int bytes_recieved =
      recvfrom(socket_server, buffer, sizeof(buffer), 0,
               (struct sockaddr *)&client_address, &client_len);
  printf("Received (%d bytes): %.*s\n", bytes_recieved, bytes_recieved, buffer);
  printf("Remote address is: ");
  char address_buffer[128];
  char service_buffer[128];
  getnameinfo((struct sockaddr *)&client_address, client_len, address_buffer,
              sizeof(address_buffer), service_buffer, sizeof(service_buffer),
              NI_NUMERICHOST | NI_NUMERICSERV);
  printf("%s %s\n", address_buffer, service_buffer);

  printf("Closing server...\n");
  close(socket_server);
  printf("Finished\n");

  return 0;
}
