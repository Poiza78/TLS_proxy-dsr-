#ifndef CONNECTION_H
#define CONNECTION_H

#include <openssl/ssl.h> // SSL_*

#define BUF_SIZE 1024
#define MAX_EVENTS 10
#define SERVER 0
#define CLIENT 1

struct fds {
	int tcp_s;
	int tls_s;
	int type;
	SSL *ssl_m;
};

void error(const char* err_msg);
int make_socket(int ip, const char* port, int type);

SSL_CTX* init_CTX(const SSL_METHOD *(*TLS_method)(void),
		const char *cert, const char *key, const char *ca);
int verificate(SSL *ssl, char *common_name);

int set_nonblocking(int sock);
int SSL_nonblock(int (*SSL_F)(),
		SSL *ssl, char *buf, int buf_len);

#endif
