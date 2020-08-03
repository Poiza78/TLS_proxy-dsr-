#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
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
	set_nonblocking(tls_serv_sock);
	if (listen(tls_serv_sock, SOMAXCONN) < 0)
		error("listen");

	int ready, efd;
	struct epoll_event event, *events;
	struct fds fds, *fds_buf, *fds_s;

	if ((efd = epoll_create1(0)) < 0)
		error("epoll_create1");
	event.events = EPOLLIN;

	fds.tls_s = tls_serv_sock;
	fds.tcp_s = -1;
	fds.ssl_m = 0;
	fds.type = SERVER;
	event.data.ptr = &fds;

	if (epoll_ctl(efd, EPOLL_CTL_ADD, tls_serv_sock, &event) < 0)
		error("epoll_ctl");

	events = calloc(MAX_EVENTS, sizeof *events);

	while(1){
		ready = epoll_wait(efd, events, MAX_EVENTS, -1);
		if (ready < 0) error("epoll_wait");

		for (int i=0; i < ready; ++i){
			fds_buf = events[i].data.ptr;

			if ((events[i].events & EPOLLERR)
			  ||(events[i].events & EPOLLHUP)){
				fprintf (stderr, "epoll error\n");
				goto TLS_error;
			}
			if (tls_serv_sock == fds_buf->tls_s){
				//TODO handle new connection

				SSL *ssl;

				tls_client_sock = accept(tls_serv_sock, 0, 0);
				if (tls_client_sock < 0){
					perror("accept");
					continue;
				}
				set_nonblocking(tls_client_sock);

				tcp_serv_sock = make_socket(INADDR_LOOPBACK, argv[1], CLIENT);
				set_nonblocking(tcp_serv_sock);

				ssl = SSL_new(ctx);
				//i dont know why, but this function
				//is initial regardless server method in init_ctx
				SSL_set_accept_state(ssl);
				SSL_set_fd(ssl, tls_client_sock);

				fds_s = calloc(2, sizeof *fds_s);

				fds_s[0].tcp_s = tcp_serv_sock;
				fds_s[0].tls_s = tls_client_sock;
				fds_s[0].ssl_m = ssl;
				fds_s[0].type = CLIENT;

				event.data.ptr = &fds_s[0];
				event.events = EPOLLIN;

				if (epoll_ctl(efd, EPOLL_CTL_ADD, tls_client_sock, &event) < 0)
					error("epoll_ctl");

				memcpy(&fds_s[1], &fds_s[0], sizeof (struct fds));
				fds_s[1].type = SERVER;
				event.data.ptr = &fds_s[1];
				event.events = EPOLLOUT;

				if (epoll_ctl(efd, EPOLL_CTL_ADD, tcp_serv_sock, &event) < 0)
					error("epoll_ctl");

			} else {
				if (!SSL_is_init_finished(fds_buf->ssl_m)){
					if (SSL_do_handshake_nonblock(fds_buf->ssl_m) <= 0){
						fprintf(stderr, "ssl_accept error\n");
						goto TLS_error;
					}
					if (!verificate(fds_buf->ssl_m, CN)){
						fprintf(stderr, "verification error\n");
						goto TLS_error;
					}
				}
				char buf[BUF_SIZE];
				int ret;
				memset(buf, 0, sizeof buf);

				if (events[i].events & EPOLLIN){
					ret = SSL_read_all(fds_buf->ssl_m, buf, sizeof buf);

					if (ret <= 0)
						goto TLS_error;

					write(fds_buf->tcp_s, buf, ret);
				} else
				if (events[i].events & EPOLLOUT){
					ret = read(fds_buf->tcp_s, buf, sizeof buf);

					ret = SSL_write_all(fds_buf->ssl_m, buf, ret);

					if (ret <= 0)
						goto TLS_error;
				}
			}
		continue;
		TLS_error:
		SSL_free(fds_buf->ssl_m);
		close (fds_buf->tcp_s);
		close (fds_buf->tls_s);
		free (fds_buf);
		}
	}

	SSL_CTX_free(ctx);
	close(tls_serv_sock);
	exit(EXIT_SUCCESS);
}

