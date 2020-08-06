#include "connection.h"
#include <stdio.h> // perror()
#include <stdlib.h> // exit()
#include <string.h> // memset()
#include <netinet/in.h> // hton()
#include <sys/socket.h> // socket(), bind(),..   ???
#include <sys/epoll.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include <fcntl.h>
#include <unistd.h>

void error(const char *err_msg)
{
		perror (err_msg);
		exit (EXIT_FAILURE);
}

int make_socket(int ip, const char* port, int type)
{
	int sock;
	struct sockaddr_in addr;
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		error("socket");

	memset(&addr,0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons (atoi(port));
	addr.sin_addr.s_addr = htonl (ip);

	switch (type){
	case SERVER:
		if (bind (sock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
			error("bind");
		break;
	case CLIENT:
		if (connect(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
			error ("connect");
		break;
	}
	return sock;
}

void TLS_error()
{
	ERR_print_errors_fp(stderr);
	exit(EXIT_FAILURE);
}

SSL_CTX* init_CTX(const SSL_METHOD *(*TLS_method)(void),
		const char *cert, const char *key, const char *ca)
{
	/*
	* As of version 1.1.0 OpenSSL will automatically allocate all 
	* resources that it needs so no explicit initialisation is required.
	*/
	const SSL_METHOD *method;
	SSL_CTX *ctx;

	method = TLS_method();
	ctx = SSL_CTX_new(method);
	if (!ctx)
		TLS_error();
	if ( SSL_CTX_use_certificate_file(ctx, cert, SSL_FILETYPE_PEM) <= 0 )
		TLS_error();
	if ( SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) <= 0 )
		TLS_error();
	if ( !SSL_CTX_check_private_key(ctx)) {
		fprintf(stderr, "Key does not match the certificate\n");
		exit(EXIT_FAILURE);
	}
	if (!SSL_CTX_load_verify_locations(ctx, ca, ca))
		TLS_error();

	// second flag ignored on client side 
	SSL_CTX_set_verify(ctx,	SSL_VERIFY_PEER, 0);

	return ctx;
}

int verificate(SSL *ssl, char *common_name)
{
	X509* cert = SSL_get_peer_certificate(ssl);
	if (!cert){
		fprintf(stderr, "no certificate!\n");
		return -1;
	}

	int ret = SSL_get_verify_result(ssl);
	if (ret != X509_V_OK){
		fprintf(stderr, "wrong certificate!\n");
		return -1;
	}

	ret = X509_check_host(cert, common_name, 0,0,0);
	X509_free(cert);

	return (ret > 0);
}

int set_nonblocking(int sock)
{
	int flags = fcntl(sock, F_GETFL, 0);
	if (flags < 0) {
		perror("fcntl_get");
		return -1;
	}
	if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
		perror("fcntl_set");
		return -1;
	}
	return 0;
}

int SSL_nonblock(int (*SSL_IO)(),
		SSL *ssl, char *buf, int buf_len)
{
	int ret, efd;
	struct epoll_event event, *events;

	efd = epoll_create1(0);
	if (efd == -1){
		perror("epoll_create1");
		goto out;
	}

	event.events = EPOLLIN | EPOLLOUT;
	event.data.fd = SSL_get_fd(ssl);
	ret = epoll_ctl(efd, EPOLL_CTL_ADD, event.data.fd, &event);
	if (ret == -1){
		perror("epoll_ctl");
		goto out;
	}
	events = calloc(1, sizeof *events);
	while((ret = SSL_IO(ssl, buf, buf_len)) <= 0){

		switch(SSL_get_error(ssl, ret)){
		case SSL_ERROR_WANT_READ:
			event.events = EPOLLIN;
			epoll_ctl(efd, EPOLL_CTL_MOD, event.data.fd, &event);
			break;
		case SSL_ERROR_WANT_WRITE:
			event.events = EPOLLOUT;
			epoll_ctl(efd, EPOLL_CTL_MOD, event.data.fd, &event);
			break;
		case SSL_ERROR_SYSCALL:
			fprintf(stderr,"SSL_ERROR_SYSCALL\n");
			goto out;
		case SSL_ERROR_SSL:
			fprintf(stderr,"SSL_ERROR_SSL\n");
			TLS_error();
			break;
		default:
			goto out;
		}

		epoll_wait(efd, events, 1, -1);
	}
out:
	free(events);
	close(efd);
	return ret;
}

void init_data(struct data_t *data, int tls_s, int tcp_s, SSL* ssl)
{
	data->tls_s = tls_s;
	data->tcp_s = tcp_s;

	data->read_size = BUF_SIZE;
	data->write_size = BUF_SIZE;

	data->read_total = 0;
	data->write_total = 0;

	data->read_buf = calloc(data->read_size, sizeof (char));
	data->write_buf = calloc(data->write_size, sizeof (char));

	data->ssl_state = SSL_WANT_HANDSHAKE;

	data->ssl = ssl;
}

void cleanup_connection(connection_t *conn, int efd)
{
	struct data_t *data = conn->data;

	if (data != NULL){
		free(data->read_buf);
		free(data->write_buf);

		data->read_buf = NULL;
		data->write_buf = NULL;

		epoll_ctl(efd, EPOLL_CTL_DEL, data->tls_s, 0);
		epoll_ctl(efd, EPOLL_CTL_DEL, data->tcp_s, 0);

		close(data->tls_s);
		close(data->tcp_s);

		SSL_free(conn->data->ssl);
	}
	if (SERVER == conn->type) conn--;
	free(conn);
	conn = NULL;
}

static int handle_state(connection_t *conn, int ret, int efd)
{
	struct epoll_event event = {0};

	event.data.ptr = conn;

	switch(SSL_get_error(conn->data->ssl, ret)){
	case SSL_ERROR_NONE:
		conn->data->ssl_state = SSL_OK;
		event.events = EPOLLIN;
		epoll_ctl(efd, EPOLL_CTL_MOD, conn->data->tls_s, &event);
		return 1;
	case SSL_ERROR_WANT_READ:
		event.events = EPOLLIN;
		epoll_ctl(efd, EPOLL_CTL_MOD, conn->data->tls_s, &event);
		return 0;
	case SSL_ERROR_WANT_WRITE:
		event.events = EPOLLOUT;
		epoll_ctl(efd, EPOLL_CTL_MOD, conn->data->tls_s, &event);
		return 0;
	default:
		return -1;
	}
}

int do_SSL_handshake(connection_t *conn, int efd)
{
	SSL *ssl = conn->data->ssl;
	int ret, err;

	ret = SSL_do_handshake(ssl);
	err = handle_state(conn, ret, efd);

	return err;
}

int do_SSL_read(connection_t *conn, int efd)
{
	SSL *ssl;
	char *buf;
	size_t *size, *total;
	int ret, err;

	ssl = conn->data->ssl;
	buf = conn->data->read_buf;
	size = &(conn->data->read_size);
	total = &(conn->data->read_total);

	/* ?? undeclared ?? */
	//ret = SSL_read_ex(ssl, buf, *size, total); 
	ret = SSL_read(ssl, buf+*total, *size);
	err = handle_state(conn, ret, efd);

	if (err <= 0){
		conn->data->ssl_state = SSL_WANT_PERFORM_READ;
		return err;
	}

	*total += ret;
	if (*total == *size){
		//TODO remake this ...
		*size <<= 1;
		buf = realloc(buf, *size);
	}
	return 1;
}

int do_SSL_write(connection_t *conn, int efd)
{
	return 1;
}

int do_read(connection_t *conn, int efd)
{
	return 1;
}

int do_write(connection_t *conn, int efd)
{
	return 1;
}
