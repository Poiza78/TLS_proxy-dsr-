#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/epoll.h>
#include "connection.h"

#define CERT "./client_cert/tls_client.crt"
#define KEY "./client_cert/tls_client.key"
#define CA "./server_cert/ca.crt"
#define CN "tls_server"

int main(int argc, const char** argv)
{
	if (argc != 3){
		fprintf(stderr,
			"Usage: ./tls_client tls_serv_port listen_port\n");
		exit(EXIT_FAILURE);
	}

	SSL_CTX *ctx;
	int tls_serv_sock, tls_client_sock, tcp_client_sock;

	ctx = init_CTX(TLSv1_2_client_method, CERT, KEY, CA);

	tls_client_sock = make_socket(INADDR_ANY, argv[2], SERVER);
	if (listen(tls_client_sock, SOMAXCONN) < 0)
		error("listen");

	while(1){
		SSL *ssl;

		tcp_client_sock = accept(tls_client_sock, 0, 0);
		if (tcp_client_sock < 0){
			perror("accept");
			continue;
		}
		set_nonblocking(tcp_client_sock);

		tls_serv_sock = make_socket(INADDR_LOOPBACK, argv[1], CLIENT);
		set_nonblocking(tls_serv_sock);

		ssl = SSL_new(ctx);
		SSL_set_connect_state(ssl);
		SSL_set_fd(ssl, tls_serv_sock);

		int ready, efd;
		struct epoll_event event, *events;

		if ((efd = epoll_create1(0)) < 0)
			error("epoll_create1");

		event.events = EPOLLIN;
		event.data.fd = tcp_client_sock;

		if (epoll_ctl(efd, EPOLL_CTL_ADD, tcp_client_sock, &event) < 0)
			error("epoll_ctl");

		event.events = EPOLLIN;
		event.data.fd = tls_serv_sock;

		if (epoll_ctl(efd, EPOLL_CTL_ADD, tls_serv_sock, &event) < 0)
			error("epoll_ctl");

		events = calloc(2, sizeof *events);

		while(1){

			ready = epoll_wait(efd, events, 1, -1);
			if (ready < 0) error("epoll_wait");

			for (int i=0; i < ready; ++i){

				if ((events[i].events & EPOLLERR)
				  ||(events[i].events & EPOLLHUP)
				  ||(!(events[i].events & EPOLLIN))){
					fprintf (stderr, "epoll error\n");
					goto TLS_error;
				}
				if (!SSL_is_init_finished(ssl)){
					if (SSL_nonblock(SSL_do_handshake, ssl, 0, 0) <= 0){
						fprintf(stderr, "ssl_accept error\n");
						goto TLS_error;
					}
					if (!verificate(ssl, CN)){
						fprintf(stderr, "verification error\n");
						goto TLS_error;
					}
				}
				char buf[BUF_SIZE];
				int ret;
				memset(buf, 0, sizeof buf);

				if (tcp_client_sock == events[i].data.fd){
					ret = read(tcp_client_sock, buf, sizeof buf);
					ret = SSL_nonblock(SSL_write, ssl, buf, ret);

					if (ret <= 0)
						goto TLS_error;

				} else if (tls_serv_sock == events[i].data.fd){
					ret = SSL_nonblock(SSL_read, ssl, buf, sizeof buf);

					if (ret <= 0)
						goto TLS_error;

					write(tcp_client_sock, buf, ret);
				}
			}
		}
		TLS_error:
		SSL_free(ssl);
		free(events);
		close (tcp_client_sock);
		close (tls_serv_sock);
		close (efd);
	}
	SSL_CTX_free(ctx);
	close(tls_client_sock);
	exit(EXIT_SUCCESS);
}
