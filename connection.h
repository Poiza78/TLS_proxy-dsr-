#ifndef CONNECTION_H
#define CONNECTION_H

#include <openssl/ssl.h> // SSL_*

#define BUF_SIZE 1024
#define MAX_EVENTS 10
#define SERVER 0
#define CLIENT 1

enum ssl_state
{
	SSL_WANT_HANDSHAKE,
	SSL_OK,
	SSL_WANT_PERFORM_READ,
	SSL_WANT_PERFORM_WRITE
};

typedef struct
{
	struct{
		int tcp_s;
		int tls_s;

		enum ssl_state ssl_state;

		size_t read_len;
		size_t write_len;

		char *read_buf;
		char *write_buf;

		SSL *ssl;

	} *data;

	int type;

} connection_t;

void init_ssl_connection(connection_t *conn, int tls_s, int tcp_s, SSL* ssl);
void cleanup_ssl_connection(connection_t *conn);

void error(const char* err_msg);

int make_socket(int ip, const char* port, int type);
int set_nonblocking(int sock);

SSL_CTX* init_CTX(const SSL_METHOD *(*TLS_method)(void),
		const char *cert, const char *key, const char *ca);

int verificate(SSL *ssl, char *common_name);


#endif
