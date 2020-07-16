#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include "tcp_connection.c"
#include "calculation.c"
#include <unistd.h> // close()

#define QUEUE_SIZE 3

int main(int argc, const char** argv){
	if (argc != 2){
		fprintf(stderr, "Usage: ./tcp_server port_num\n");
		exit(EXIT_FAILURE);
	}
	int server_sock;
	server_sock = make_socket(atoi(argv[1]), INADDR_ANY);
	if (listen(server_sock, QUEUE_SIZE) == -1){
		perror("listen");
		exit(EXIT_FAILURE);
	}
	while(1){
		struct sockaddr_in client_addr;
		int addr_len = sizeof (client_addr);
		int connection_sock = accept(server_sock,
					(struct sockaddr*) &client_addr,
					&addr_len);	
		if (connection_sock == -1){
			perror("accept");
			exit(EXIT_FAILURE);
		}

		process(connection_sock);
		close(connection_sock); 
	}
	exit(EXIT_SUCCESS);
}
