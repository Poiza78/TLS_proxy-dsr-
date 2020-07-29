#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
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

	while(1){
		SSL *ssl;
		char buf[BUF_SIZE];
		int len;

		tcp_client_sock = accept(tls_client_sock, 0, 0);
		if (tcp_client_sock < 0){
			perror("accept");
			continue;
		}

		tls_serv_sock = make_socket(INADDR_LOOPBACK, argv[1], CLIENT);

		ssl = SSL_new(ctx);
		SSL_set_fd(ssl, tls_serv_sock);

		if (SSL_connect(ssl)  <= 0) {
			ERR_print_errors_fp(stderr);
			goto TLS_error;
		}
		if (!verificate(ssl, CN)){
			fprintf(stderr, "verification error\n");
			goto TLS_error;
		}
		while(1){
			memset(buf,0, sizeof(buf));
			if (recv(tcp_client_sock, buf, BUF_SIZE, 0) < 0){
				perror("recv");
				goto TLS_error;
			}
			if (SSL_write(ssl, buf, strlen(buf)) <= 0){
				ERR_print_errors_fp(stderr);
				goto TLS_error;
			}
			if ((len = SSL_read(ssl, buf, BUF_SIZE)) <= 0){
				ERR_print_errors_fp(stderr);
				goto TLS_error;
			}
			if (send(tcp_client_sock, buf, len, 0) < 0){
				perror("send");
				goto TLS_error;
			}
		}
		TLS_error:
		SSL_free(ssl);
		close(tcp_client_sock);
		close(tls_serv_sock);
	}
	SSL_CTX_free(ctx);
	close(tls_client_sock);
	exit(EXIT_SUCCESS);
}
