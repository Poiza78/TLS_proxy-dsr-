#ifndef TCP_CONNECTION
#define TCP_CONNECTION

#include <stdio.h> // perror()
#include <stdlib.h> // exit()
#include <netinet/in.h> // hton()
#include <sys/socket.h>
#include <string.h> // memset()

int make_socket(int port, int addr){
	int sock;
	struct sockaddr_in params;
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0){
		perror("socket");
		exit(EXIT_FAILURE);
	}
	memset(&params,0, sizeof(params));	
	params.sin_family = AF_INET;
	params.sin_port = htons (port);
	params.sin_addr.s_addr = htonl (addr);
	if (bind (sock, (struct sockaddr *) &params, sizeof (params)) < 0){
		perror ("bind");
		exit (EXIT_FAILURE);
	}
  	return sock;
}

#endif
