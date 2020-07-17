#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include "tcp_connection.c"
#include "calculation.c"

int main(int argc, const char** argv){
	if (argc != 2){
		fprintf(stderr, "Usage: ./tcp_server port_num\n");
		exit(EXIT_FAILURE);
	}
	int server_sock, client_sock;
	server_sock = make_socket(argv[1], INADDR_ANY, SERVER);
	while(1){
		client_sock = accept(server_sock, 0,0);
		if (client_sock < 0)
			error("accept");
		process(client_sock);
		close(client_sock); 
	}
	exit(EXIT_SUCCESS);
}
