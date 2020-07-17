#ifndef TCP_CONNECTION
#define TCP_CONNECTION

#include <stdio.h> // perror()
#include <stdlib.h> // exit()
#include <netinet/in.h> // hton()
#include <sys/socket.h>
#include <string.h> // memset()

void error(const char *err_msg){
		perror (err_msg);
		exit (EXIT_FAILURE);
}
int make_socket(int port, int ip)
{
	int sock;
	struct sockaddr_in addr;
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		error("socket");
	memset(&addr,0, sizeof(addr));	
	addr.sin_family = AF_INET;
	addr.sin_port = htons (port);
	addr.sin_addr.s_addr = htonl (ip);
	if (bind (sock, (struct sockaddr *) &addr, sizeof (addr)) < 0)
		error("bind");
  	return sock;
}


#endif
