#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/epoll.h>
#include "connection.h"

#define CERT "./server_cert/tls_server.crt"
#define KEY "./server_cert/tls_server.key"
#define CA "./server_cert/ca.crt"
#define CN "tls_client"

#define MAX_EVENTS 10

int main(int argc, const char** argv)
{
	if (argc != 3){
		fprintf(stderr,
			"Usage: ./tls_server tcp_serv_port listen_port\n");
		exit(EXIT_FAILURE);
	}

	SSL_CTX *ctx;
	int tls_serv_sock, tcp_serv_sock, tls_client_sock;

	ctx = init_CTX(TLSv1_2_server_method, CERT, KEY, CA);

	tls_serv_sock = make_socket(INADDR_ANY, argv[2], SERVER);
	if (listen(tls_serv_sock, SOMAXCONN) < 0)
		error("listen");

	while(1){
		SSL *ssl;
		char buf[BUF_SIZE];
		int len;

		tls_client_sock = accept(tls_serv_sock, 0, 0);
		if (tls_client_sock < 0){
			perror("accept");
			continue;
		}

		ssl = SSL_new(ctx);
		SSL_set_fd(ssl, tls_client_sock);

		if (SSL_accept(ssl) <= 0) {
			ERR_print_errors_fp(stderr);
			goto TLS_error;
		}
		if (!verificate(ssl, CN)){
			fprintf(stderr, "verification error\n");
			goto TLS_error;
		}
		tcp_serv_sock = make_socket(INADDR_LOOPBACK, argv[1], CLIENT);
		set_nonblocking(tls_serv_sock);
		set_nonblocking(tls_client_sock);

		int ready, efd;
		struct epoll_event event, *events;
		struct fds fds[2], *fds_buf, *fds_s;

		if ((efd = epoll_create1(0)) < 0)
			error("epoll_create1");
		event.events = EPOLLIN;

		fds[0].tls_s = tls_client_sock;
		fds[0].tcp_s = tcp_serv_sock;
		fds[0].ssl_m = ssl;
		fds[0].type = CLIENT;

		event.data.ptr = &fds[0];

		if (epoll_ctl(efd, EPOLL_CTL_ADD, tls_client_sock, &event) < 0)
			error("epoll_ctl");

		memcpy(&fds[1], &fds[0], sizeof (struct fds));
		fds[1].type = SERVER;
		event.data.ptr = &fds[1];

		if (epoll_ctl(efd, EPOLL_CTL_ADD, tcp_serv_sock, &event) < 0)
			error("epoll_ctl");

		events = calloc(MAX_EVENTS, sizeof *events);
		fds_s = calloc(MAX_EVENTS, sizeof *fds_s);

		while(1){
			ready = epoll_wait(efd, events, MAX_EVENTS, -1);
			if (ready < 0) error("epoll_wait");
			for (int i=0; i < ready; ++i){
				fds_buf = events[i].data.ptr;
				if (events[i].events & EPOLLIN){
					char buf[BUF_SIZE];
					int len;
					memset(buf, 0, sizeof buf);
					if (fds_buf->type == CLIENT){
						len = SSL_read(fds_buf->ssl_m, buf, sizeof buf);
						if (len < 0){
						//handle error: close connection,etc
						}
						write(fds_buf->tcp_s, buf, len);
					} else
					if (fds_buf->type == SERVER){
						len = read(fds_buf->tcp_s, buf, sizeof buf);
						if (len < 0){
						//handle error: close connection,etc
						}
						SSL_write(fds_buf->ssl_m, buf, len);
					}
				}
			}
		}
		TLS_error:
		SSL_free(ssl);
		close(tls_client_sock);
		close(tcp_serv_sock);
	}
	SSL_CTX_free(ctx);
	close(tls_serv_sock);
	exit(EXIT_SUCCESS);
}
