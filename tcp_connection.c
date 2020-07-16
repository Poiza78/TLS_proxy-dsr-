#ifndef TCP_CONNECTION
#define TCP_CONNECTION

#include <stdio.h> // perror()
#include <stdlib.h> // exit()
#include <netinet/in.h> // hton()
#include <sys/socket.h>
#include <string.h> // memset()

int make_socket(int port, int ip){
	int sock;
	struct sockaddr_in addr;
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0){
		perror("socket");
		exit(EXIT_FAILURE);
	}
	memset(&addr,0, sizeof(addr));	
	addr.sin_family = AF_INET;
	addr.sin_port = htons (port);
	addr.sin_addr.s_addr = htonl (ip);
	if (bind (sock, (struct sockaddr *) &addr, sizeof (addr)) < 0){
		perror ("bind");
		exit (EXIT_FAILURE);
	}
  	return sock;
}

#endif
