#pragma once

#include <sys/socket.h>

#define MAX_REQUEST_SIZE 2047

struct client_info {
  socklen_t address_length;
  struct sockaddr_storage address;
  char address_buffer[128];
  int socket;
  char request[MAX_REQUEST_SIZE + 1];
  int received;
  struct client_info *next;
};

/*
 * It gets the client_info related to the socket. Creates a entry at the head of
 * the linked list if not present.
 */
struct client_info *get_client(struct client_info **clients, int socket);

void drop_client(struct client_info **clients,
                 struct client_info *client_to_remove);

const char *get_client_address(struct client_info *client_info);
