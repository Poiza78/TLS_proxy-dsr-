#TODO
#CC=gcc
#CFLAGS= -ljansson -lreadline

all: tcp_server tcp_client tls_server

tcp_server: tcp_server.o connection.o
	gcc tcp_server.o connection.o -o tcp_server -ljansson -lssl -lcrypto

tls_server: tls_server.o connection.o
	gcc tls_server.o connection.o -o tls_server -lssl -lcrypto 

tcp_client: tcp_client.o connection.o
	gcc tcp_client.o connection.o -o tcp_client -ljansson -lreadline -lssl -lcrypto

connection.o: connection.c connection.h
	gcc -c connection.c -o connection.o 

tcp_server.o: tcp_server.c 
	gcc -c -g tcp_server.c -o tcp_server.o 

tls_server.o: tls_server.c 
	gcc -c tls_server.c -o tls_server.o 

tcp_client.o: tcp_client.c 
	gcc -c -g tcp_client.c -o tcp_client.o 

clean: 
	rm *.o tcp_server tcp_client tls_server
