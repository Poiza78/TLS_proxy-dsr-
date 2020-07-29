#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "connection.h"

#define CERT "./server_cert/tls_server.crt"
#define KEY "./server_cert/tls_server.key"
#define CA "./server_cert/ca.crt"
#define CN "tls_client"

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
		while(1){
			memset(buf,0, sizeof(buf));
			if (SSL_read(ssl, buf, BUF_SIZE) <= 0){
				ERR_print_errors_fp(stderr);
				goto TLS_error;
			}
			if (send(tcp_serv_sock, buf, strlen(buf), 0) < 0){
				perror("send");
				goto TLS_error;
			}
			if ((len = recv(tcp_serv_sock, buf, BUF_SIZE, 0)) < 0){
				perror("recv");
				goto TLS_error;
			}
			if (SSL_write(ssl, buf, len) <= 0){
				ERR_print_errors_fp(stderr);
				goto TLS_error;
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
