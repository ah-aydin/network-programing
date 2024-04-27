#include <arpa/inet.h>
#include <bits/types/timer_t.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define SOCKET int

volatile sig_atomic_t execute = 1;
void trap(int signal) {
  execute = 0;
}

int main() {
  signal(SIGINT, trap);

  printf("Configurtion local address...\n");

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  struct addrinfo *bind_address;
  if (getaddrinfo(NULL, "8082", &hints, &bind_address) != 0) {
    fprintf(stderr, "getaddrsinfo failed");
    return 1;
  }

  printf("Creating socket...\n");
  SOCKET socket_server =
      socket(bind_address->ai_family, bind_address->ai_socktype,
             bind_address->ai_protocol);
  if (socket_server < 0) {
    fprintf(stderr, "Failed to create socket (%d)", errno);
    freeaddrinfo(bind_address);
    return 1;
  }

  printf("Binding socket to local address...\n");
  if (bind(socket_server, bind_address->ai_addr, bind_address->ai_addrlen) !=
      0) {
    fprintf(stderr, "Failed to bind (%d)\n", errno);
    freeaddrinfo(bind_address);
    return 1;
  }
  freeaddrinfo(bind_address);

  printf("Listening...\n");
  if (listen(socket_server, 16) != 0) {
    fprintf(stderr, "Failed to listen (%d)", errno);
    return 1;
  }

  while (execute) {
    printf("Waiting for connection...\n");
    struct sockaddr_storage client_address;
    socklen_t client_len = sizeof(client_address);
    SOCKET socket_client =
        accept(socket_server, (struct sockaddr *)&client_address, &client_len);
    if (socket_client < 0) {
      fprintf(stderr, "Failed to accept connection (%d)", errno);
      continue;
    }

    char address_buffer[128];
    getnameinfo((struct sockaddr *)&client_address, client_len, address_buffer,
                sizeof(address_buffer), NULL, 0, NI_NUMERICHOST);
    printf("Client '%s' is connected...\n", address_buffer);

    printf("Reading request...\n");
    char request[1024];
    int bytes_recieved = recv(socket_client, request, sizeof(request), 0);
    // TODO check if bytes_recieved >= 0 in PROD
    printf("%.*s", bytes_recieved, request);

    printf("Sending response...\n");
    const char *response = "HTTP/1.1 200 OK\r\n"
                           "Connection: close\r\n"
                           "Content0Type:text/plain\r\n\r\n"
                           "Local time is: ";
    int bytes_sent = send(socket_client, response, (int)strlen(response), 0);
    // TODO check if bytes_sent >= 0 in PROD
    printf("Sent %d of %d bytes\n", bytes_sent, (int)strlen(response));

    time_t timer;
    time(&timer);
    char *time_msg = ctime(&timer);
    bytes_sent = send(socket_client, time_msg, (int)strlen(time_msg), 0);
    printf("Sent %d of %d bytes\n", bytes_sent, (int)strlen(time_msg));

    printf("Closing conection... \n");
    close(socket_client);
  }

  printf("Closing server socket...\n");
  close(socket_server);

  printf("Finished\n");

  return 0;
}
