all: https_simple openssl_version

https_simple: https_simple.c
	gcc https_simple.c -lcrypto -lssl -o build/https_simple

openssl_version: openssl_version.c
	gcc openssl_version.c -lcrypto -o build/openssl_version
