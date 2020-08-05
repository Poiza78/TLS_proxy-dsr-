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
	set_nonblocking(tls_serv_sock);
	if (listen(tls_serv_sock, SOMAXCONN) < 0)
		error("listen");

	int ready, efd;
	struct epoll_event *events, event = {0};
	//struct fds fds, *fds_buf, *fds_s;

	if ((efd = epoll_create1(0)) < 0)
		error("epoll_create1");
	event.events = EPOLLIN;

	if (epoll_ctl(efd, EPOLL_CTL_ADD, tls_serv_sock, &event) < 0)
		error("epoll_ctl");

	events = calloc(MAX_EVENTS, sizeof *events);

	while(1){

	ready = epoll_wait(efd, events, MAX_EVENTS, -1);
	if (ready < 0){
		perror("epoll_wait");
		break;
	}

	for (int i=0; i<ready; ++){

		conn_buf = events[i].data.ptr;

		if (( events[i].events & EPOLLERR )
		  ||( events[i].events & EPOLLHUP )
		  ||(!( events[i].events & EPOLLIN )
		  && !( events[i].events & EPOLLOUT )))
		{
			// TODO handle closed connection 
			// close sockets, free buffers etc
		} else if (tls_serv_sock == conn_buf->data->tls_s){
			// TODO handle new connection

		} else if (CLIENT == conn_buf->type){
			switch (conn_buf->data->ssl_state){
			case SSL_OK:
				if (events[i].events & EPOLLIN){
					// TODO do_SSL_read();
				} else {// if (events[i].events & EPOLLOUT
					// TODO do_SSL_write();
				}
				break;
			case SSL_WANT_HANDSHAKE:
				//TODO do_SSL_handshake();
				break;
			case SSL_WANT_PERFORM_READ:
				do_SSL_read();
				break;
			case SSL_WANT_PERFORM_WRITE:
				do_SSL_write();
				break;
			default:
				//unreachable 


			}
		} else { // if (SERVER == conn_buf->type)

			if (events[i].events & EPOLLIN){
				//TODO do_read();

			} else { // if (events[i].events & EPOLLOUT)
				//TODO do_write();
				// maybe w/o dedicated functions? 

			}


		}
		continue;
TLS_error:

	} // event_loop



	} // while (1)

	SSL_CTX_free(ctx);
	close(tls_serv_sock);
	close(efd);
	exit(EXIT_SUCCESS);
}

