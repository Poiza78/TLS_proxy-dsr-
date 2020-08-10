#ifndef CONNECTION_H
#define CONNECTION_H

#include <openssl/ssl.h> // SSL_*

#define BUF_SIZE 1024
#define MAX_EVENTS 10
#define SERVER 0
#define CLIENT 1
#define LISTENER 2

enum ssl_state
{
	SSL_FAIL = -1,
	SSL_OK,
	SSL_WANT_PERFORM_READ,
	SSL_WANT_PERFORM_WRITE,
	SSL_WANT_HANDSHAKE,
	SSL_WANT_IO
};

typedef struct
{
	struct data_t{
		int tcp_s;
		int tls_s;

		enum ssl_state ssl_state;

		size_t read_size, read_total;
		size_t write_size, write_total;

		char *read_buf;
		char *write_buf;

		SSL *ssl;

	} *data;

	int type;

} connection_t;

void *init_connection(int tls_s, int tcp_s, SSL* ssl);
void cleanup_connection(connection_t *conn, int efd);

int make_socket(int ip, const char* port, int type);
int set_nonblocking(int sock);
void error(const char* err_msg);

SSL_CTX* init_CTX(const SSL_METHOD *(*TLS_method)(void),
		const char *cert, const char *key, const char *ca);

int verificate(connection_t *conn, char *common_name);

int do_SSL_handshake(connection_t *conn, int efd, char *common_name);
int do_SSL_read(connection_t *conn, int efd);
int do_SSL_write(connection_t *conn, int efd);
int do_read(connection_t *conn, int efd);
int do_write(connection_t *conn, int efd);

#endif
