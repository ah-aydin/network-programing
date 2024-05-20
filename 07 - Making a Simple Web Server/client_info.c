#include "client_info.h"

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

struct client_info *get_client(struct client_info **clients, int s) {
  struct client_info *current_client = *clients;
  while (current_client) {
    if (current_client->socket == s) {
      return current_client;
    }
    current_client = current_client->next;
  }

  struct client_info *new_client =
      (struct client_info *)calloc(1, sizeof(struct client_info));
  if (!new_client) {
    fprintf(stderr, "Out of memory.\n");
    exit(1);
  }

  new_client->address_length = sizeof(new_client->address);
  new_client->next = *clients;
  sprintf(new_client->address_buffer, "");
  *clients = new_client;

  return new_client;
}

void drop_client(struct client_info **clients,
                 struct client_info *client_to_remove) {
  close(client_to_remove->socket);

  struct client_info **p = clients;
  while (*p) {
    if (*p == client_to_remove) {
      *p = client_to_remove->next;
      free(client_to_remove);
      return;
    }
    p = &(*p)->next;
  }

  fprintf(stderr, "drop_client not found.\n");
  exit(1);
}

const char *get_client_address(struct client_info *client_info) {
  if (strlen(client_info->address_buffer) != 0) {
    return client_info->address_buffer;
  }

  getnameinfo((struct sockaddr *)&client_info->address,
              client_info->address_length, client_info->address_buffer,
              sizeof(client_info->address_buffer), 0, 0, NI_NUMERICHOST);
  return client_info->address_buffer;
}
