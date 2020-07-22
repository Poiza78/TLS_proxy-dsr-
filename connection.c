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
SSL_CTX* init_CTX(SSL_METHOD *(*TLS_method)(void), char *cert, char *key)
{
	const SSH_METHOD *method;
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
	// cannot fail
	// second flag ignored on client side 
	SSL_CXT_set_verify(ctx, 
		SSL_FERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, 0);

	return ctx;
}
void verificate(SSL *ssl)
{
	//TODO
}
