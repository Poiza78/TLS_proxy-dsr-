#ifndef TCP_CONNECTION
#define TCP_CONNECTION

#include <stdio.h> // perror()
#include <stdlib.h> // exit()
#include <netinet/in.h> // hton()
#include <string.h> // memset()
#include <sys/socket.h> // socket(), bind(),..   ???

#define QUEUE_SIZE 3

enum{
	SERVER = 0,
	CLIENT = 1,
};

void error(const char *err_msg){
		perror (err_msg);
		exit (EXIT_FAILURE);
}
int make_socket(const char* port, int ip, int type)
{
	int sock, len;
	struct sockaddr_in addr;
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		error("socket");
	memset(&addr,0, sizeof(addr));	
	addr.sin_family = AF_INET;
	addr.sin_port = htons (atoi(port));
	addr.sin_addr.s_addr = htonl (ip);
	len = sizeof(addr);
	switch (type){
	case SERVER:
		if (bind (sock, (struct sockaddr *) &addr, len) < 0)
			error("bind");
		if (listen(sock, QUEUE_SIZE) < 0)
			error("listen");
		break;
	case CLIENT:
		if (connect(sock, (struct sockaddr *) &addr, len) < 0)
			perror ("connect");
		break;
	}
  	return sock;
}

#endif
