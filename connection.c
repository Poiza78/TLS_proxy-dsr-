#include "connection.h"
#include <stdio.h> // perror()
#include <stdlib.h> // exit()
#include <string.h> // memset()
#include <netinet/in.h> // hton()
#include <sys/socket.h> // socket(), bind(),..   ???

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

//TODO
//SSL_CTX* init_CTX(SSL_METHOD *(*method)(void)){}
//void configure_cert(SSL_CTX* ctx, char* file, char* key){}
