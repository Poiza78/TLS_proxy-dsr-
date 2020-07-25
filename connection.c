#include "connection.h"
#include <stdio.h> // perror()
#include <stdlib.h> // exit()
#include <string.h> // memset()
#include <netinet/in.h> // hton()
#include <sys/socket.h> // socket(), bind(),..   ???
#include <openssl/ssl.h>
#include <openssl/err.h>

void error(const char *err_msg)
{
		perror (err_msg);
		exit (EXIT_FAILURE);
}

int make_socket(int ip, const char* port, int type)
{
	int sock;
	struct sockaddr_in addr;
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		error("socket");

	memset(&addr,0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons (atoi(port));
	addr.sin_addr.s_addr = htonl (ip);

	switch (type){
	case SERVER:
		if (bind (sock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
			error("bind");
		if (listen(sock, QUEUE_SIZE) < 0)
			error("listen");
		break;
	case CLIENT:
		if (connect(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
			error ("connect");
		break;
	}
	return sock;
}

void TLS_error()
{
	ERR_print_errors_fp(stderr);
	exit(EXIT_FAILURE);
}

/*
* As of version 1.1.0 OpenSSL will automatically allocate all 
* resources that it needs so no explicit initialisation is required.
* Similarly it will also automatically deinitialise as required.
*/
SSL_CTX* init_CTX(const SSL_METHOD *(*TLS_method)(void), const char *cert, const char *key)
{
	const SSL_METHOD *method;
	SSL_CTX *ctx;

	method = TLS_method();
	ctx = SSL_CTX_new(method);
	if (!ctx)
		TLS_error();
	if ( SSL_CTX_use_certificate_file(ctx, cert, SSL_FILETYPE_PEM) <= 0 )
		TLS_error();
	if ( SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) <= 0 )
		TLS_error();
	if ( !SSL_CTX_check_private_key(ctx)) {
		fprintf(stderr, "Private key does not match the public certificate\n");
		exit(EXIT_FAILURE);
	}
	// second flag ignored on client side 
	SSL_CTX_set_verify(ctx,
		SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, 0);

	return ctx;
}
int verificate(SSL *ssl)
{
	X509* cert = SSL_get_peer_certificate(ssl);
	if (cert) X509_free(cert);  // unneeded
	if (!cert) return 0;

	int res = SSL_get_verify_result(ssl);
	if(!(X509_V_OK == res)) return 0;

	return 1;
}
