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
	int efd, ready, ret;
	struct epoll_event events[2], event = {0};
	connection_t *conn_buf;


	ctx = init_CTX(TLSv1_2_client_method, CERT, KEY, CA);

	tls_client_sock = make_socket(INADDR_ANY, argv[2], SERVER);

	if (listen(tls_client_sock, SOMAXCONN) < 0)
		error("listen");

	if ((efd = epoll_create1(0)) < 0)
		error("epoll_create1");

	while(1){

		tcp_client_sock = accept(tls_client_sock, 0, 0);
		if (tcp_client_sock < 0){
			perror("accept");
			continue;
		}
		set_nonblocking(tcp_client_sock);

		tls_serv_sock = make_socket(INADDR_LOOPBACK, argv[1], CLIENT);
		set_nonblocking(tls_serv_sock);

		SSL *ssl = SSL_new(ctx);
		SSL_set_connect_state(ssl);
		SSL_set_fd(ssl, tls_serv_sock);

		conn_buf = init_connection(tls_serv_sock, tcp_client_sock, ssl);

		event.events = EPOLLIN;
		event.data.ptr = conn_buf;

		ret = epoll_ctl(efd, EPOLL_CTL_ADD, tcp_client_sock, &event);
		if (ret < 0){
			perror("epoll_ctl");
			goto out;
		}

		event.events = EPOLLIN;
		event.data.ptr = conn_buf+1; // &conn_buf[1]

		ret = epoll_ctl(efd, EPOLL_CTL_ADD, tls_serv_sock, &event);
		if (ret < 0){
			perror("epoll_ctl");
			goto out;
		}
		while (SSL_WANT_HANDSHAKE == do_SSL_handshake(conn_buf, efd,CN))
			epoll_wait(efd, events, 1, -1);

		if (SSL_FAIL == conn_buf->data->ssl_state){
			fprintf(stderr, "unsuccessful connection\n");
			goto out;
		}

		while(1){

		ready = epoll_wait(efd,events, 2, -1);
		if (ready < 0)
			error("epoll_wait");

		for (int i=0; i<ready; ++i){

			conn_buf = events[i].data.ptr;

			if (( events[i].events & EPOLLERR )
			  ||( events[i].events & EPOLLHUP )
			  ||(!( events[i].events & EPOLLIN )
		          && !( events[i].events & EPOLLOUT )))
			{
				goto out;
			} else
			if (CLIENT == conn_buf->type){

				if ((events[i].events & EPOLLIN)
				&& (ret = do_read(conn_buf, efd)) > 0)
				{
					//add epollout to tls_server
					event.events = EPOLLIN | EPOLLOUT;
					event.data.ptr = conn_buf+1;
					ret = epoll_ctl(efd,
							EPOLL_CTL_MOD,
							conn_buf->data->tls_s,
							&event);
				} else
				if ((events[i].events & EPOLLOUT)
				&& (ret = do_write(conn_buf, efd)) > 0)
				{
					//remove epollout from tcp_client
					event.events = EPOLLIN;
					event.data.ptr = conn_buf;
					ret = epoll_ctl(efd,
							EPOLL_CTL_MOD,
							conn_buf->data->tcp_s,
							&event);
				}
				if (ret < 0) goto out;

			} else {// type == SERVER

			enum ssl_state *state = &(conn_buf->data->ssl_state);

			if( (SSL_WANT_PERFORM_READ == *state
			  ||(SSL_OK == *state && (events[i].events & EPOLLIN)))
			  && (SSL_OK == do_SSL_read(conn_buf, efd)))
			// if ( (1 or (2 and 3)) and 4 )
			{
				//add epollout to tcp_client
				event.events = EPOLLIN | EPOLLOUT;
				event.data.ptr = conn_buf-1;
				ret = epoll_ctl(efd,
						EPOLL_CTL_MOD,
						(conn_buf)->data->tcp_s,
						&event);
				if (ret < 0){
					perror("epoll_ctl");
					goto out;
				}
			} else
			if( (SSL_WANT_PERFORM_WRITE == *state
			  ||(SSL_OK == *state && (events[i].events & EPOLLOUT)))
			  && (SSL_OK == do_SSL_write(conn_buf, efd)))
			{
				//remove epollout from tls_server
				event.events = EPOLLIN;
				event.data.ptr = conn_buf;
				ret = epoll_ctl(efd,
						EPOLL_CTL_MOD,
						(conn_buf)->data->tls_s,
						&event);
				if (ret < 0){
					perror("epoll_ctl");
					goto out;
				}
			}
			if (SSL_FAIL == *state) // fatal error during secure I/O 
				goto out; // or break? 
			} // type == SERVER
		} // event loop
		} // nested while(1)
out:
		cleanup_connection(conn_buf, efd);
	}
	SSL_CTX_free(ctx);
	close(tls_client_sock);
	close(efd);
	exit(EXIT_SUCCESS);
}
