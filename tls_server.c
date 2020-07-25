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

int main(int argc, const char** argv)
{
	if (argc != 3){
		fprintf(stderr,
			"Usage: ./tls_server tcp_serv_port listen_port\n");
		exit(EXIT_FAILURE);
	}

	SSL_CTX *ctx;
	int tls_serv_sock, tcp_serv_sock, tls_client_sock;

	tcp_serv_sock = make_socket(INADDR_LOOPBACK, argv[1], CLIENT);
	tls_serv_sock = make_socket(INADDR_ANY, argv[2], SERVER);

	ctx = init_CTX(TLSv1_2_server_method, CERT, KEY);
	SSL_CTX_load_verify_locations(ctx, CA, CA);

	while(1){
		SSL *ssl;
		char buf[BUF_SIZE];
		int len;

		tls_client_sock = accept(tls_serv_sock, 0, 0);
		if (tls_client_sock < 0)
			error("accept");

		ssl = SSL_new(ctx);
		SSL_set_fd(ssl, tls_client_sock);

		if (SSL_accept(ssl) <= 0) {
			ERR_print_errors_fp(stderr);
			goto TLS_error;
		}
		if (!verificate(ssl)){
			fprintf(stderr, "verification error\n");
			goto TLS_error;
		}
		while(1){
			memset(buf,0, sizeof(buf));
			if (SSL_read(ssl, buf, BUF_SIZE) <= 0){
				ERR_print_errors_fp(stderr);
				goto TLS_error;
			}
			if (send(tcp_serv_sock, buf, strlen(buf), 0) < 0){
				error("send");
				goto TLS_error;
			}
			if (recv(tcp_serv_sock, buf, BUF_SIZE, 0) < 0){
				error("recv");
				goto TLS_error;
			}
			if (SSL_write(ssl, buf, strlen(buf)) <= 0){
				ERR_print_errors_fp(stderr);
				goto TLS_error;
			}
		}
		TLS_error:
		SSL_free(ssl);
		close(tls_client_sock);
	}
	SSL_CTX_free(ctx);
	close(tcp_serv_sock);
	close(tls_serv_sock);
	exit(EXIT_SUCCESS);
}
