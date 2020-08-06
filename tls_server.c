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
	int tls_serv_sock, tls_client_sock;
	int efd, ready, ret;
	struct epoll_event *events, event = {0};
	connection_t *conn_buf;

	ctx = init_CTX(TLSv1_2_server_method, CERT, KEY, CA);

	tls_serv_sock = make_socket(INADDR_ANY, argv[2], SERVER);
	set_nonblocking(tls_serv_sock);
	if (listen(tls_serv_sock, SOMAXCONN) < 0)
		error("listen");

	if ((efd = epoll_create1(0)) < 0)
		error("epoll_create1");


	conn_buf = calloc(1, sizeof *conn_buf);
	conn_buf->type = LISTENER;

	event.events = EPOLLIN;
	event.data.ptr = conn_buf;
	conn_buf = NULL;

	if (epoll_ctl(efd, EPOLL_CTL_ADD, tls_serv_sock, &event) < 0)
		error("epoll_ctl");

	events = calloc(MAX_EVENTS, sizeof *events);

	while(1){

	ready = epoll_wait(efd, events, MAX_EVENTS, -1);
	if (ready < 0){
		perror("epoll_wait");
		break;
	}

	for (int i=0; i<ready; ++i){

		conn_buf = events[i].data.ptr;

		if (( events[i].events & EPOLLERR )
		  ||( events[i].events & EPOLLHUP )
		  ||(!( events[i].events & EPOLLIN )
		  && !( events[i].events & EPOLLOUT )))
		{
			cleanup_connection(conn_buf, efd);

		} else if (LISTENER == conn_buf->type){
			//handle new connection
			SSL* ssl;

			tls_client_sock = accept(tls_serv_sock, 0, 0);
			if (tls_client_sock < 0){
				perror("accept");
				continue;
			}
			set_nonblocking(tls_client_sock);

			ssl = SSL_new(ctx);
			SSL_set_accept_state(ssl);
			SSL_set_fd(ssl, tls_client_sock);

			conn_buf = calloc(1, sizeof *conn_buf);
			conn_buf->type = CLIENT;
			conn_buf->data = calloc(1, sizeof *(conn_buf->data));
			init_data(conn_buf->data, tls_client_sock, -1, ssl);

			event.events = EPOLLIN;
			event.data.ptr = conn_buf;

			ret = epoll_ctl(
				efd, EPOLL_CTL_ADD, tls_client_sock, &event);
			if (ret < 0){
				perror("epoll_ctl");
				cleanup_connection(conn_buf, efd);
			}
			conn_buf = NULL;

		} else if (CLIENT == conn_buf->type){
			switch (conn_buf->data->ssl_state){
			case SSL_OK:
				if (events[i].events & EPOLLIN){
					ret = do_SSL_read(conn_buf, efd);

				} else {// if (events[i].events & EPOLLOUT
					ret = do_SSL_write(conn_buf, efd);
				}
				break;
			case SSL_WANT_HANDSHAKE:
				if ((ret = do_SSL_handshake(conn_buf, efd)) > 0
				&&(ret=verificate(conn_buf->data->ssl,CN)) > 0){

					connection_t *tmp;

					conn_buf->data->tcp_s =	make_socket(
								INADDR_LOOPBACK,
								argv[1], CLIENT);
					set_nonblocking(conn_buf->data->tcp_s);

					tmp = calloc(1, sizeof *tmp);
					tmp->type = SERVER;
					tmp->data = conn_buf->data;

					event.events = EPOLLIN;
					event.data.ptr = tmp;

					ret = epoll_ctl(efd,
							EPOLL_CTL_ADD,
							tmp->data->tcp_s,
							&event);
					if (ret < 0){
						perror("epoll_ctl");
						cleanup_connection(tmp, efd);
					}
					tmp = NULL;
					continue;
				}
				break;
			// next 2 cases for non-application data
			case SSL_WANT_PERFORM_READ:
				ret = do_SSL_read(conn_buf, efd);
				break;
			case SSL_WANT_PERFORM_WRITE:
				ret = do_SSL_write(conn_buf, efd);
				break;
			case SSL_FAIL:
				break;
			}
			if (ret < 0) // fatal error during I/O 
				cleanup_connection(conn_buf, efd);

		} else { // if (SERVER == conn_buf->type)

			if (events[i].events & EPOLLIN){
				//TODO do_read();

			} else { // if (events[i].events & EPOLLOUT)
				//TODO do_write();
				// maybe w/o dedicated functions? 

			}


		}
	} // event_loop
	} // while (1)

	SSL_CTX_free(ctx);
	close(tls_serv_sock);
	close(efd);
	exit(EXIT_SUCCESS);
}

