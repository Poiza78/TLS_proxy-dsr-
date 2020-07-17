all: tcp_server tcp_client

tcp_server: tcp_server.o tcp_connection.o calculation.o
	gcc tcp_server.o -o tcp_server -ljansson

tcp_client: tcp_client.o tcp_connection.o
	gcc tcp_client.o -o tcp_client -ljansson -lreadline

tcp_server.o: tcp_server.c 
	gcc -c tcp_server.c -o tcp_server.o 

tcp_client.o: tcp_client.c 
	gcc -c tcp_client.c -o tcp_client.o 

tcp_connection.o: tcp_connection.c
	gcc -c tcp_connection.c -o tcp_connection.o 

calculation.o: calculation.c
	gcc -c calculation.c -o calculation.o

clean: 
	rm *.o tcp_server tcp_client
