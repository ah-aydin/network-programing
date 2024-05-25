#include <openssl/crypto.h>
#include <openssl/ssl.h>

int main() {
  printf("OpenSSL version:%s\n", OPENSSL_VERSION_TEXT);
  return 0;
}
