/*
 * For reference
 * https://en.wikipedia.org/wiki/Domain_Name_System#:~:text=address%20changes%20administratively.-,DNS%20message%20format,content%20of%20these%20four%20sections.
 */

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
#include <unistd.h>

const unsigned char *print_name(const unsigned char *msg,
                                const unsigned char *p,
                                const unsigned char *end) {
  if (p + 2 > end) {
    fprintf(stderr, "End of message.\n");
    exit(1);
  }

  if ((*p & 0xC0) == 0xC0) {
    const int k = ((*p & 0x3F) << 8) + p[1];
    p += 2;
    printf(" (pointer %d) ", k);
    print_name(msg, msg + k, end);
    return p;
  }

  const int len = *p++;
  if (p + len + 1 > end) {
    fprintf(stderr, "End of message.\n");
    exit(1);
  }

  printf("%.*s", len, p);
  p += len;

  if (*p) {
    printf(".");
    return print_name(msg, p, end);
  }
  return p + 1;
}

void print_dns_message(const char *message, int msg_length) {
  if (msg_length < 12) {
    fprintf(stderr, "Message is too short to be valid.\n");
    exit(1);
  }

  const unsigned char *msg = (const unsigned char *)message;
  /*for (int i = 0; i < msg_length; ++i) {*/
  /*  unsigned char c = msg[i];*/
  /*  printf("%02d: %02X %03d '%c'\n", i, c, c, c);*/
  /*}*/
  /*printf("\n");*/

  printf("Id = %0x %0X \n", msg[0], msg[1]);

  const int qr = (msg[2] & 0x80) >> 7;
  printf("QR = %d %s\n", qr, qr ? "response" : "query");

  const int opcode = (msg[2] & 0x78) >> 3;
  printf("OPCODE %d ", opcode);
  switch (opcode) {
  case 0:
    printf("standard\n");
    break;
  case 1:
    printf("inverse\n");
    break;
  case 2:
    printf("status\n");
    break;
  default:
    printf("?\n");
  }

  const int aa = (msg[2] & 0x4) >> 2;
  printf("AA %d %s\n", aa, aa ? "authoritataive" : "");

  const int tc = (msg[2] & 0x2) >> 1;
  printf("TC %d %s\n", tc, tc ? "message truncated" : "");

  const int rd = (msg[2] & 0x1);
  printf("RD %d %s\n", rd, rd ? "recursion desired" : "");

  if (qr) {
    const int rcode = msg[3] & 0xF;
    printf("RCODE = %d ", rcode);
    switch (rcode) {
    case 0:
      printf("success\n");
      break;
    case 1:
      printf("format error\n");
      break;
    case 2:
      printf("server failure\n");
      break;
    case 3:
      printf("nonexistent domain\n");
      break;
    case 4:
      printf("not implemented\n");
      break;
    case 5:
      printf("refused\n");
      break;
    default:
      printf("?\n");
    }

    if (rcode != 0)
      return;
  }

  const int qdcount = (msg[4] << 8) + msg[5];
  const int ancount = (msg[6] << 8) + msg[7];
  const int nscount = (msg[8] << 8) + msg[9];
  const int arcount = (msg[10] << 8) + msg[11];

  printf("QDCOUNT = %d\n", qdcount);
  printf("ANCOUNT= %d\n", ancount);
  printf("NSCOUNT = %d\n", nscount);
  printf("ARCOUNT = %d\n", arcount);

  const unsigned char *p = msg + 12;
  const unsigned char *end = msg + msg_length;

  // Questions
  for (int i = 0; i < qdcount; ++i) {
    if (p >= end) {
      fprintf(stderr, "End of message.\n");
      exit(1);
    }

    printf("Query %2d\n", i + 1);
    printf("\tname: ");
    p = print_name(msg, p, end);
    printf("\n");

    if (p + 4 > end) {
      fprintf(stderr, "End of message.\n");
      exit(1);
    }

    const int type = (p[0] << 8) + p[1];
    printf("\ttype: %d\n", type);
    p += 2;

    const int qclass = (p[0] << 8) + p[1];
    printf("\tclass: %d\n", qclass);
    p += 2;
  }

  // Answers
  for (int i = 0; i < ancount; ++i) {
    if (p >= end) {
      fprintf(stderr, "End of message.\n");
      exit(1);
    }

    printf("Answer %2d\n", i + 1);
    printf("\tname");
    p = print_name(msg, p, end);
    printf("\n");

    if (p + 10 > end) {
      fprintf(stderr, "End of message.\n");
      exit(1);
    }

    const int type = (p[0] << 8) + p[1];
    printf("\ttype: %d\n", type);
    p += 2;

    const int qclass = (p[0] << 8) + p[1];
    printf("\tclass: %d\n", qclass);
    p += 2;

    const unsigned int ttl = (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
    printf("\tttl: %u\n", ttl);
    p += 4;

    const int rdlen = (p[0] << 8) + p[1];
    printf("\trdlen: %d\n", rdlen);
    p += 2;

    if (p + rdlen > end) {
      fprintf(stderr, "End of message.\n");
      exit(1);
    }

    // Type A
    if (rdlen == 4 && type == 1) {
      printf("\tAddress %d.%d.%d.%d\n", p[0], p[1], p[2], p[3]);
    } else if (type == 15 && rdlen > 3) { // Type MX
      const int preference = (p[0] << 8) + p[1];
      printf("\tMX: ");
      print_name(msg, p + 2, end);
      printf("\n");
    } else if (rdlen == 16 && type == 28) { // Type AAAA
      printf("\tAddress ");
      for (int j = 0; j < rdlen; j += 2) {
        printf("%02x%02x", p[j], p[j + 1]);
        if (j + 2 < rdlen) {
          printf(":");
        }
      }
      printf("\n");
    } else if (type == 16) { // Type TXT
      printf("\tTXT: '%.*s'\n", rdlen - 1, p + 1);
    } else if (type == 5) { // Type CNAME
      printf("\tCNAME: ");
      print_name(msg, p, end);
      printf("\n");
    }
    p += rdlen;
  }

  if (p != end) {
    printf("There is lefover data.\n");
  }
  printf("\n");
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    printf("Usage:\n\tdns_query hostname type\n");
    printf("Example:\n\tdns_query example.com aaaa\n");
  }

  if (strlen(argv[1]) > 255) {
    fprintf(stderr, "Hostname too long.");
    return 1;
  }

  unsigned char type;
  char *type_c = argv[2];
  if (strcmp(type_c, "a") == 0) {
    type = 1;
  } else if (strcmp(type_c, "mx") == 0) {
    type = 15;
  } else if (strcmp(type_c, "txt") == 0) {
    type = 16;
  } else if (strcmp(type_c, "aaaa") == 0) {
    type = 28;
  } else if (strcmp(type_c, "any") == 0) {
    type = 255;
  } else {
    fprintf(stderr, "Unsupported type '%s'. Use a, aaaa, txt, mx or any.",
            type_c);
    return 1;
  }

  printf("Configurint remote address...\n");
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_DGRAM;
  struct addrinfo *peer_address;
  if (getaddrinfo("8.8.8.8", "53", &hints, &peer_address)) {
    fprintf(stderr, "getaddrinfo() failed. (%d)\n", errno);
    return 1;
  }

  printf("Creating socket...\n");
  int socket_peer;
  socket_peer = socket(peer_address->ai_family, peer_address->ai_socktype,
                       peer_address->ai_protocol);
  if (!socket_peer) {
    fprintf(stderr, "socket() failed. (%d)\n", errno);
    return 1;
  }

  char query[1024] = {
      0xAB, 0xCD, // ID
      0x01, 0x00, // Set recursion
      0x00, 0x01, // Question count
      0x00, 0x00, // Answer count
      0x00, 0x00, // NSCOUNT
      0x00, 0x00, // ARCOUNT
  };

  char *p = query + 12;
  char *h = argv[1];

  while (*h) {
    char *len = p;
    p++;
    if (h != argv[1]) {
      h++;
    }

    while (*h && *h != '.') {
      *p++ = *h++;
    }
    *len = p - len - 1;
  }

  *p++ = 0;

  *p++ = 0x00;
  *p++ = type;
  *p++ = 0x00;
  *p++ = 0x01;

  const int query_size = p - query;

  printf("Sending query:\n");
  print_dns_message(query, query_size);
  printf("\n");
  int bytes_sent = sendto(socket_peer, query, query_size, 0,
                          peer_address->ai_addr, peer_address->ai_addrlen);
  printf("Sent %d byets.\n", bytes_sent);

  printf("\n\n\n");

  char buffer[1024];
  int bytes_received =
      recvfrom(socket_peer, buffer, sizeof(buffer), 0, NULL, NULL);
  printf("Received %d bytes.\n", bytes_received);

  print_dns_message(buffer, bytes_received);

  freeaddrinfo(peer_address);
  close(socket_peer);

  return 0;
}
