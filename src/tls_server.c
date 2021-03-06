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
	connection_t *conn_buf, *conn;

	ctx = init_CTX(TLSv1_2_server_method, CERT, KEY, CA);

	tls_serv_sock = make_socket(INADDR_ANY, argv[2], SERVER);
	set_nonblocking(tls_serv_sock);
	if (listen(tls_serv_sock, SOMAXCONN) < 0)
		error("listen");

	if ((efd = epoll_create1(0)) < 0)
		error("epoll_create1");


	conn = calloc(1, sizeof *conn_buf);
	conn->data = NULL;
	conn->type = LISTENER;

	event.events = EPOLLIN;
	event.data.ptr = conn;

	if (epoll_ctl(efd, EPOLL_CTL_ADD, tls_serv_sock, &event) < 0)
		error("epoll_ctl");

	events = calloc(MAX_EVENTS, sizeof *events);

	while(1){

	ready = epoll_wait(efd, events, MAX_EVENTS, -1);
	if (ready < 0)
		error("epoll_wait");

	for (int i=0; i<ready; ++i){

		conn_buf = events[i].data.ptr;

		if (NULL == conn_buf)
			continue;

		if (( events[i].events & EPOLLERR )
		  ||( events[i].events & EPOLLHUP )
		  ||(!( events[i].events & EPOLLIN )
		  && !( events[i].events & EPOLLOUT )))
		{
			cleanup_connection(conn_buf, efd);

		} else
		switch(conn_buf->type){
		case LISTENER:
		{
			//handle new connection
			tls_client_sock = accept(tls_serv_sock, 0, 0);
			if (tls_client_sock < 0){
				perror("accept");
				continue;
			}
			set_nonblocking(tls_client_sock);

			SSL *ssl = SSL_new(ctx);
			SSL_set_accept_state(ssl);
			SSL_set_fd(ssl, tls_client_sock);

			conn_buf = init_connection(tls_client_sock, -1, ssl);

			event.events = EPOLLIN;
			event.data.ptr = conn_buf;

			ret = epoll_ctl(
				efd, EPOLL_CTL_ADD, tls_client_sock, &event);
			if (ret < 0){
				perror("epoll_ctl");
				cleanup_connection(conn_buf, efd);
			}
			break;
		}
		case CLIENT:
		{
			enum ssl_state *state = &(conn_buf->data->ssl_state);
			if (SSL_WANT_HANDSHAKE == *state
			&& SSL_OK == do_SSL_handshake(conn_buf, efd, CN))
			{
				conn_buf->data->tcp_s =	make_socket(
							INADDR_LOOPBACK,
							argv[1], CLIENT);
				set_nonblocking(conn_buf->data->tcp_s);

				event.events = EPOLLIN;
				event.data.ptr = conn_buf+1;

				ret = epoll_ctl(efd,
						EPOLL_CTL_ADD,
						conn_buf->data->tcp_s,
						&event);
				if (ret < 0){
					perror("epoll_ctl");
					cleanup_connection(conn_buf, efd);
				}
			} else
			if( (SSL_WANT_PERFORM_READ == *state
			  ||(SSL_OK == *state && (events[i].events & EPOLLIN)))
			  && (SSL_OK == do_SSL_read(conn_buf, efd)))
			// if ( (1 or (2 and 3)) and 4 )
			{
				//add epollout to tcp_server
				event.events = EPOLLIN | EPOLLOUT;
				event.data.ptr = conn_buf+1;
				ret = epoll_ctl(efd,
						EPOLL_CTL_MOD,
						(conn_buf)->data->tcp_s,
						&event);
				if (ret < 0){
					perror("epoll_ctl");
					cleanup_connection(conn_buf, efd);
				}
			} else
			if( (SSL_WANT_PERFORM_WRITE == *state
			  ||(SSL_OK == *state && (events[i].events & EPOLLOUT)))
			  && (SSL_OK == do_SSL_write(conn_buf, efd)))
			{
				//remove epollout from tls_client
				event.events = EPOLLIN;
				event.data.ptr = conn_buf;
				ret = epoll_ctl(efd,
						EPOLL_CTL_MOD,
						(conn_buf)->data->tls_s,
						&event);
				if (ret < 0){
					perror("epoll_ctl");
					cleanup_connection(conn_buf, efd);
				}
			}
			if (SSL_FAIL == *state) // fatal error during secure I/O 
				cleanup_connection(conn_buf, efd);
			break;
		}
		case SERVER:
		{
			if ((events[i].events & EPOLLIN)
			&& (ret = do_read(conn_buf, efd)) > 0)
			{
				//add epollout to tls_client
				event.events = EPOLLIN | EPOLLOUT;
				event.data.ptr = conn_buf-1;
				ret = epoll_ctl(efd,
						EPOLL_CTL_MOD,
						conn_buf->data->tls_s,
						&event);
				/* 
				if (ret < 0){
					perror("epoll_ctl");
					cleanup_connection(conn_buf, efd);
				}
				*/
			} else
			if ((events[i].events & EPOLLOUT)
			&& (ret = do_write(conn_buf, efd)) > 0)
			{
				//remove epollout from tcp_server
				event.events = EPOLLIN;
				event.data.ptr = conn_buf;
				ret = epoll_ctl(efd,
						EPOLL_CTL_MOD,
						conn_buf->data->tcp_s,
						&event);
				/* 
				if (ret < 0){
					perror("epoll_ctl");
					cleanup_connection(conn_buf, efd);
				}
				*/
			}
			if (ret < 0) cleanup_connection(conn_buf, efd);
		} // case SERVER
		} // switch(conn_buf->type)
	} // event_loop
	} // while (1)

	free(conn);
	SSL_CTX_free(ctx);
	close(tls_serv_sock);
	close(efd);
	exit(EXIT_SUCCESS);
}

