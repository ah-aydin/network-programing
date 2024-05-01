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

#define PORT "8081"

int main(int argc, char *argv[]) {
  printf("Configuring local address...\n");

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  struct addrinfo *bind_address;
  if (getaddrinfo(NULL, PORT, &hints, &bind_address)) {
    eprintf("getaddrinfo() failed (%d)\n", errno);
    return 1;
  }

  printf("Creating socket...\n");
  SOCKET socket_server;
  socket_server = socket(bind_address->ai_family, bind_address->ai_socktype,
                         bind_address->ai_protocol);
  if (socket_server < 0) {
    eprintf("socket() failed (%d)\n", errno);
    freeaddrinfo(bind_address);
    return 1;
  }

  printf("Binding socket to local address...\n");
  if (bind(socket_server, bind_address->ai_addr, bind_address->ai_addrlen) !=
      0) {
    eprintf("bind() failed (%d)\n", errno);
    freeaddrinfo(bind_address);
    return 1;
  }
  freeaddrinfo(bind_address);

  printf("Listening...\n");
  if (listen(socket_server, 16) < 0) {
    eprintf("listen() failed (%d)\n", errno);
    return 1;
  }

  SOCKET max_socket = socket_server;
  fd_set master;
  FD_ZERO(&master);
  FD_SET(socket_server, &master);

  printf("Waiting for connections...\n");

  while (1) {
    fd_set reads;
    reads = master;
    if (select(max_socket + 1, &reads, NULL, NULL, NULL) < 0) {
      eprintf("select() failed (%d)\n", errno);
      return 1;
    }

    for (SOCKET socket = 1; socket <= max_socket; ++socket) {
      if (!FD_ISSET(socket, &reads)) {
        continue;
      }
      if (socket == socket_server) {
        struct sockaddr_storage client_address;
        socklen_t client_len = sizeof(client_address);
        SOCKET socket_client = accept(
            socket_server, (struct sockaddr *)&client_address, &client_len);
        if (socket < 0) {
          eprintf("accept() failed (%d)\n", errno);
          return 1;
        }

        FD_SET(socket_client, &master);
        if (socket_client > max_socket) { // New connection
          max_socket = socket_client;
        }

        char address_buffer[128];
        getnameinfo((struct sockaddr *)&client_address, client_len,
                    address_buffer, sizeof(address_buffer), 0, 0,
                    NI_NUMERICHOST);
        printf("New connection from %s\n", address_buffer);
      } else { // Existing connection
        char read_buffer[1024];
        int bytes_recieved = recv(socket, read_buffer, 1024, 0);
        if (bytes_recieved < 1) {
          FD_CLR(socket, &master);
          close(socket);
          continue;
        }

        for (int index = 0; index < bytes_recieved; ++index) {
          read_buffer[index] = toupper(read_buffer[index]);
        }
        send(socket, read_buffer, bytes_recieved, 0);
      }
    }
  }

  printf("Closing server socket...\n");
  close(socket_server);
  printf("Finished\n");

  return 0;
}
