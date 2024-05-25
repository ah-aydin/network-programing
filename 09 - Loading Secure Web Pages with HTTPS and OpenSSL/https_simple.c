#include <errno.h>
#include <netdb.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/tls1.h>
#include <openssl/x509.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int exit_code = EXIT_SUCCESS;

int main(int argc, char *argv[]) {
  SSL_library_init();
  OpenSSL_add_ssl_algorithms();
  SSL_load_error_strings();

  SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
  if (!ctx) {
    fprintf(stderr, "SSL_CTX_new() failed.\n");
    return 1;
  }

  if (argc < 3) {
    fprintf(stderr, "usage: https_simple hostname port\n");
    exit_code = EXIT_FAILURE;
    goto cleanup;
  }

  char *hostname = argv[1];
  char *port = argv[2];

  printf("Configurint remote addres...\n");
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;

  struct addrinfo *peer_address;
  if (getaddrinfo(hostname, port, &hints, &peer_address)) {
    fprintf(stderr, "getaddrinfo() failed. (%d)\n", errno);
    exit_code = EXIT_FAILURE;
    goto cleanup;
  }

  printf("Remote address is:");
  char address_buffer[128];
  char service_buffer[128];
  if (getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen,
                  address_buffer, sizeof(address_buffer), service_buffer,
                  sizeof(service_buffer), NI_NUMERICHOST | NI_NUMERICSERV)) {
    fprintf(stderr, "getnameinfo() failed. (%d)\n", errno);
    freeaddrinfo(peer_address);
    exit_code = EXIT_FAILURE;
    goto cleanup;
  }
  printf("%s %s\n", address_buffer, service_buffer);

  printf("Creating socket...\n");
  int socket_server = socket(peer_address->ai_family, peer_address->ai_socktype,
                             peer_address->ai_protocol);
  if (socket_server == -1) {
    printf("socket() failed. (%d)\n", errno);
    freeaddrinfo(peer_address);
    exit_code = EXIT_FAILURE;
    goto cleanup;
  }

  printf("Connecting...\n");
  if (connect(socket_server, peer_address->ai_addr, peer_address->ai_addrlen)) {
    printf("connect() failed. (%d)\n", errno);
    freeaddrinfo(peer_address);
    exit_code = EXIT_FAILURE;
    goto cleanup;
  }
  freeaddrinfo(peer_address);

  printf("Connected!\n\n");

  SSL *ssl = SSL_new(ctx);
  if (!ssl) {
    fprintf(stderr, "SSL_new() failed.\n");
    exit_code = EXIT_FAILURE;
    goto cleanup;
  }

  if (!SSL_set_tlsext_host_name(ssl, hostname)) {
    fprintf(stderr, "SSL_set_tlsext_host_name() failed.\n");
    ERR_print_errors_fp(stderr);
    exit_code = EXIT_FAILURE;
    goto cleanup;
  }

  SSL_set_fd(ssl, socket_server);
  if (SSL_connect(ssl) == -1) {
    fprintf(stderr, "SSL_connect() failed.\n");
    ERR_print_errors_fp(stderr);
    exit_code = EXIT_FAILURE;
    goto cleanup;
  }

  printf("SSL/TLS using %s\n", SSL_get_cipher(ssl));

  X509 *cert = SSL_get_peer_certificate(ssl);
  if (!cert) {
    fprintf(stderr, "SSL_get_peer_certificate() failed.\n");
    ERR_print_errors_fp(stderr);
    exit_code = EXIT_FAILURE;
    goto cleanup;
  }
  char *tmp;
  if ((tmp = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0))) {
    printf("subject: %s\n", tmp);
    OPENSSL_free(tmp);
  }
  if ((tmp = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0))) {
    printf("issuer: %s\n", tmp);
    OPENSSL_free(tmp);
  }
  X509_free(cert);

  char buffer[4096];
  sprintf(buffer, "GET / HTTP/1.1\r\n");
  sprintf(buffer + strlen(buffer), "Host: %s:%s\r\n", hostname, port);
  sprintf(buffer + strlen(buffer), "Connection: close\r\n");
  sprintf(buffer + strlen(buffer), "User-Agent: https_simple\r\n");
  sprintf(buffer + strlen(buffer), "\r\n");

  SSL_write(ssl, buffer, strlen(buffer));
  printf("Sent Headers;\n%s", buffer);

  while (1) {
    int bytes_received = SSL_read(ssl, buffer, sizeof(buffer));
    if (bytes_received < 1) {
      printf("\nConneciton closed by peer.\n");
      break;
    }
    printf("Received (%d bytes): '%.*s'\n", bytes_received, bytes_received,
           buffer);
  }

  printf("\nClosing socket...\n");
cleanup:
  SSL_shutdown(ssl);
  SSL_free(ssl);
  close(socket_server);
  SSL_CTX_free(ctx);

  printf("Complete!\n");
  return exit_code;
}
