all: tcp_server tcp_client

tcp_server: tcp_server.o connection.o
	gcc tcp_server.o -o tcp_server -ljansson

tcp_client: tcp_client.o connection.o
	gcc tcp_client.o -o tcp_client -ljansson -lreadline

tcp_server.o: tcp_server.c 
	gcc -c tcp_server.c -o tcp_server.o 

tcp_client.o: tcp_client.c 
	gcc -c tcp_client.c -o tcp_client.o 

connection.o: connection.c
	gcc -c connection.c -o connection.o 

clean: 
	rm *.o tcp_server tcp_client
