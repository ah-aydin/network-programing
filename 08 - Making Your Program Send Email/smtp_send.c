#include <arpa/inet.h>
#include <bits/types/timer_t.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "mail_server.h"

#define MAX_INPUT 512
#define MAX_RESPONSE 1023

void get_input(const char *prompt, char *buffer) {
  printf("%s> ", prompt);

  buffer[0] = 0;
  fgets(buffer, MAX_INPUT, stdin);
  int chars_read = strlen(buffer);
  if (chars_read > 0) {
    buffer[chars_read - 1] = 0;
  }
}

void send_format(int socket_server, const char *text, ...) {
  char buffer[1024];
  va_list args;
  va_start(args, text);
  vsprintf(buffer, text, args);
  va_end(args);
  int bytes_sent = send(socket_server, buffer, strlen(buffer), 0);
  if (bytes_sent < 1) {
    fprintf(stderr, "send() failed. (%d)\n", errno);
    exit(EXIT_FAILURE);
  }

  printf("C: %s", buffer);
}

int parse_smtp_response(const char *response) {
  const char *k = response;
  if (!k[0] || !k[1] || !k[2])
    return 0;
  for (; k[3]; ++k) {
    if ((k == response || k[-1] == '\n') &&
        (isdigit(k[0]) && isdigit(k[1]) && isdigit(k[2])) && k[3] != '-' &&
        strstr(k, "\r\n")) {
      return strtol(k, 0, 10);
    }
  }
  return 0;
}

void wait_on_response(int socket_server, int expected_code) {
  char response[MAX_RESPONSE + 1];
  char *p = response;
  char *end = response + MAX_RESPONSE;
  int code = 0;

  do {
    int bytes_recieved = recv(socket_server, p, end - p, 0);
    if (bytes_recieved < 1) {
      fprintf(stderr, "Connection droped.\n");
      exit(EXIT_FAILURE);
    }
    p += bytes_recieved;
    *p = 0;
    if (p >= end) {
      fprintf(stderr, "Server respons exceeded limit of %d\n", MAX_RESPONSE);
      exit(EXIT_FAILURE);
    }

    code = parse_smtp_response(response);
  } while (code == 0);

  if (code != expected_code) {
    fprintf(stderr, "Expected code '%d' but got '%d'\n", expected_code, code);
    fprintf(stderr, "S: %s", response);
    exit(EXIT_FAILURE);
  }

  printf("S: %s", response);
}

int connect_to_host(const char *hostname, const char *port) {
  printf("Configuring remote address for '%s'...\n", hostname);
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;

  struct addrinfo *host_address;
  if (getaddrinfo(hostname, port, &hints, &host_address)) {
    fprintf(stderr, "getaddrinfo() failed. (%d)\n", errno);
    return -1;
  }

  printf("Remote address is: ");
  char address_buffer[128];
  char service_buffer[128];
  getnameinfo(host_address->ai_addr, host_address->ai_addrlen, address_buffer,
              sizeof(address_buffer), service_buffer, sizeof(service_buffer),
              NI_NUMERICHOST | NI_NUMERICSERV);
  printf("%s %s\n", address_buffer, service_buffer);

  printf("Creating socket...\n");
  int socket_server = socket(host_address->ai_family, host_address->ai_socktype,
                             host_address->ai_protocol);
  if (socket_server == -1) {
    fprintf(stderr, "socket() failed. (%d)\n", errno);
    freeaddrinfo(host_address);
    return -1;
  }

  if (connect(socket_server, host_address->ai_addr, host_address->ai_addrlen)) {
    fprintf(stderr, "connect() failed. (%d)\n", errno);
    freeaddrinfo(host_address);
    return -1;
  }

  freeaddrinfo(host_address);
  printf("Connected!\n\n");

  return socket_server;
}

int main() {
  char domain[MAX_INPUT];
  get_input("Mail domain", domain);

  int count;
  struct mail_server_info *mail_server_infos = get_mail_servers(domain, &count);
  if (count == 0) {
    fprintf(stderr, "Unknown mail domain '%s'\n", domain);
    return EXIT_FAILURE;
  }
  int socket_server = -1;
  for (int i = 0; i < count; ++i) {
    char *server_hostname = mail_server_infos[i].server_hostname;
    socket_server = connect_to_host(server_hostname, "25");
    if (socket_server != -1) {
      break;
    }
    printf("Failed to connect to '%s'%s", server_hostname,
           (i != count - 1) ? ", tyring to connect to the next one\n" : "\n");
  }
  if (socket_server == -1) {
    fprintf(stderr, "Failed to connect to all of the servers!\n");
    return 1;
  }

  wait_on_response(socket_server, 220);
  send_format(socket_server, "HELO AHAPC\r\n");
  wait_on_response(socket_server, 250);

  char sender[MAX_INPUT];
  get_input("from", sender);
  send_format(socket_server, "MAIL FROM:<%s>\r\n", sender);
  wait_on_response(socket_server, 250);

  char recipient[MAX_INPUT];
  get_input("recipient", recipient);
  send_format(socket_server, "RCPT TO:<%s>\r\n", recipient);
  wait_on_response(socket_server, 250);

  send_format(socket_server, "DATA\r\n");
  wait_on_response(socket_server, 354);

  char subject[MAX_INPUT];
  get_input("subject", subject);

  send_format(socket_server, "From:<%s>\r\n", sender);
  send_format(socket_server, "To:<%s>\r\n", recipient);
  send_format(socket_server, "Subject:%s\r\n", subject);

  time_t timer;
  time(&timer);
  struct tm *timeinfo;
  timeinfo = gmtime(&timer);
  char date[128];
  strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S +0000", timeinfo);
  send_format(socket_server, "Date:%s\r\n", date);
  send_format(socket_server, "Message-ID:<%lu@ahapc.com>\r\n", (unsigned long)time(NULL));

  send_format(socket_server, "\r\n");

  printf("Enter the body of the email, end with '.' on a line by itself.\n");

  while (1) {
    char buffer[MAX_INPUT];
    get_input("", buffer);
    send_format(socket_server, "%s\r\n", buffer);
    if (strcmp(buffer, ".") == 0) {
      break;
    }
  }

  wait_on_response(socket_server, 250);
  send_format(socket_server, "QUIT\r\n");
  wait_on_response(socket_server, 221);

  printf("Closing socket...\n");
  close(socket_server);
  printf("Done!\n");

  free(mail_server_infos);

  return EXIT_SUCCESS;
}
