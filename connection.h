#ifndef CONNECTION_H
#define CONNECTION_H

#include <openssl/ssl.h>

#define QUEUE_SIZE 3
#define SERVER 0
#define CLIENT 1

void error(const char* err_msg);
int make_socket(int ip, const char* port, int type);

SSL_CTX* init_CTX(SSL_METHOD *(*TLS_method)(void),
		const char *cert, const char *key);
void verificate(SSL *ssl);

#endif
