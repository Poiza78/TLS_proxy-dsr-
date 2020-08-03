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
	SSL *ssl_m;
	int type;
};

void error(const char* err_msg);
int make_socket(int ip, const char* port, int type);

SSL_CTX* init_CTX(const SSL_METHOD *(*TLS_method)(void),
		const char *cert, const char *key, const char *ca);
int verificate(SSL *ssl, char *common_name);

int set_nonblocking(int sock);
int SSL_do_handshake_nonblock(SSL *ssl);

int SSL_read_all(SSL *ssl, char *buf, int buf_len);
int SSL_write_all(SSL *ssl, char *buf, int buf_len);

#endif
