#include <arpa/inet.h>
#include <bits/types/struct_timeval.h>
#include <bits/types/timer_t.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "client_info.h"

#define RESPONSE_BUFFER_SIZE 1024
#define MAX_PATH_LENGTH 128

const char *get_content_type(const char *path) {
  const char *extension = strrchr(path, '.');
  if (extension) {
    extension++;
    if (strcmp(extension, "css") == 0) {
      return "text/css";
    }
    if (strcmp(extension, "csv") == 0) {
      return "text/csv";
    }
    if (strcmp(extension, "gif") == 0) {
      return "image/gif";
    }
    if (strcmp(extension, "htm") == 0) {
      return "text/html";
    }
    if (strcmp(extension, "html") == 0) {
      return "text/html";
    }
    if (strcmp(extension, "ico") == 0) {
      return "image/x-icon";
    }
    if (strcmp(extension, "jpeg") == 0) {
      return "image/jpeg";
    }
    if (strcmp(extension, "jpg") == 0) {
      return "image/jpeg";
    }
    if (strcmp(extension, "js") == 0) {
      return "application/javascript";
    }
    if (strcmp(extension, "json") == 0) {
      return "application/json";
    }
    if (strcmp(extension, "png") == 0) {
      return "image/png";
    }
    if (strcmp(extension, "pdf") == 0) {
      return "application/pdf";
    }
    if (strcmp(extension, "svg") == 0) {

      return "image/svg+xml";
    }
    if (strcmp(extension, "txt") == 0) {
      return "text/plain";
    }
  }

  return "application/octet-stream";
}

int create_socket(const char *host, const char *port) {
  printf("Configuring local address...\n");

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  struct addrinfo *bind_address;
  if (getaddrinfo(host, port, &hints, &bind_address)) {
    fprintf(stderr, "getaddrinfo() failed. (%d)", errno);
    exit(1);
  }

  printf("Creating socket...\n");
  int s = socket(bind_address->ai_family, bind_address->ai_socktype,
                 bind_address->ai_protocol);
  if (s == -1) {
    fprintf(stderr, "socket() failed. (%d)\n", errno);
    exit(1);
  }

  if (bind(s, bind_address->ai_addr, bind_address->ai_addrlen) == -1) {
    fprintf(stderr, "bind() failed. (%d)\n", errno);
    exit(1);
  }
  freeaddrinfo(bind_address);

  printf("Listening...\n");
  if (listen(s, 16) < 0) {
    fprintf(stderr, "listen() failed. (%d)\n", errno);
    exit(1);
  }

  return s;
}

fd_set wait_on_clients(struct client_info **clients, int socket_server) {
  fd_set reads;
  FD_ZERO(&reads);
  FD_SET(socket_server, &reads);
  int max_socket = socket_server;

  struct client_info *client = *clients;
  while (client) {
    printf("Adding client\n");
    FD_SET(client->socket, &reads);
    if (client->socket > max_socket) {
      max_socket = client->socket;
    }
    client = client->next;
  }

  if (select(max_socket + 1, &reads, NULL, NULL, NULL) < 0) {
    fprintf(stderr, "select() failed. (%d)", errno);
    exit(1);
  }

  return reads;
}

void send_400(struct client_info **clients, struct client_info *client) {
  const char *msg400 = "HTTP/1.1 400 Bad Request\r\n"
                       "Connection: close\r\n"
                       "Content-Length: 11\r\n"
                       "\r\n"
                       "Bad Request";
  send(client->socket, msg400, strlen(msg400), 0);
  drop_client(clients, client);
}

void send_404(struct client_info **clients, struct client_info *client) {
  const char *msg404 = "HTTP/1.1 404 Not Found\r\n"
                       "Connection: close\r\n"
                       "Content-Length: 9\r\n"
                       "\r\n"
                       "Not Found";
  send(client->socket, msg404, strlen(msg404), 0);
  drop_client(clients, client);
}

void serve_resource(struct client_info **clients, struct client_info *client,
                    const char *path) {
  printf("Serving %s -> %s\n", get_client_address(client), path);

  if (strcmp(path, "/") == 0) {
    path = "/index.html";
  }

  if (strlen(path) > 128) {
    send_400(clients, client);
    return;
  }

  if (strstr(path, "..")) {
    send_400(clients, client);
    return;
  }

  char full_path[MAX_PATH_LENGTH + 6];
  sprintf(full_path, "public%s", path);

  FILE *resource_fd = fopen(full_path, "rb");
  if (!resource_fd) {
    send_404(clients, client);
    return;
  }

  fseek(resource_fd, 0, SEEK_END);
  size_t resource_size = ftell(resource_fd);
  rewind(resource_fd);

  const char *content_type = get_content_type(full_path);

  char buffer[RESPONSE_BUFFER_SIZE];
  sprintf(buffer, "HTTP/1.1 200 OK\r\n");
  send(client->socket, buffer, strlen(buffer), 0);

  sprintf(buffer, "Connection: close\r\n");
  send(client->socket, buffer, strlen(buffer), 0);

  sprintf(buffer, "Conetent-Length: %zu\r\n", resource_size);
  send(client->socket, buffer, strlen(buffer), 0);

  sprintf(buffer, "Content-Type: %s\r\n", content_type);
  send(client->socket, buffer, strlen(buffer), 0);

  sprintf(buffer, "\r\n");
  send(client->socket, buffer, strlen(buffer), 0);

  int r = fread(buffer, 1, RESPONSE_BUFFER_SIZE, resource_fd);
  while (r) {
    send(client->socket, buffer, r, 0);
    r = fread(buffer, 1, RESPONSE_BUFFER_SIZE, resource_fd);
  }

  fclose(resource_fd);
  drop_client(clients, client);
}

int main() {
  struct client_info *clients = NULL;
  int socket_server = create_socket(NULL, "8080");

  while (1) {
    fd_set reads;
    printf("\nWaiting\n");
    reads = wait_on_clients(&clients, socket_server);
    printf("\nWaiting done\n");
    if (FD_ISSET(socket_server, &reads)) {
      struct client_info *client = get_client(&clients, -1);
      client->socket =
          accept(socket_server, (struct sockaddr *)&(client->address),
                 &(client->address_length));
      if (client->socket == -1) {
        fprintf(stderr, "accept() failed. (%d)\n", errno);
        return 1;
      }

      printf("New connection from '%s'\n", get_client_address(client));
    }

    struct client_info *client = clients;
    while (client) {
      struct client_info *next = client->next;
      if (FD_ISSET(client->socket, &reads)) {
        if (MAX_REQUEST_SIZE == client->received) {
          send_400(&clients, client);
          continue;
        } else {
          int r = recv(client->socket, client->request + client->received,
                       MAX_REQUEST_SIZE - client->received, 0);
          if (r < 1) {
            printf("Unexpected disconnect from %s.\n",
                   get_client_address(client));
            drop_client(&clients, client);
          } else {
            client->received += r;
            client->request[client->received] = 0;
            char *q = strstr(client->request, "\r\n\r\n");
            if (q) {
              if (strncmp("GET /", client->request, 5)) {
                send_400(&clients, client);
              } else {
                char *path = client->request + 4;
                char *end_path = strstr(path, " ");
                if (!end_path) {
                  send_400(&clients, client);
                } else {
                  *end_path = 0;
                  serve_resource(&clients, client, path);
                }
              }
            }
          }
        }
      }
      client = next;
    }
  }

  printf("Closing all connections...");
  while (clients) {
    drop_client(&clients, clients);
  }

  printf("\nClosing socket...\n");
  close(socket_server);
  printf("Done!\n");

  return 0;
}
