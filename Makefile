#TODO
#CC=gcc
#CFLAGS= -ljansson -lreadline

all: tcp_server tcp_client

tcp_server: tcp_server.o connection.o
	gcc tcp_server.o connection.o -o tcp_server -ljansson

tcp_client: tcp_client.o connection.o
	gcc tcp_client.o connection.o -o tcp_client -ljansson -lreadline 

connection.o: connection.c connection.h
	gcc -c connection.c -o connection.o 

tcp_server.o: tcp_server.c 
	gcc -c tcp_server.c -o tcp_server.o 

tcp_client.o: tcp_client.c 
	gcc -c tcp_client.c -o tcp_client.o 

clean: 
	rm *.o tcp_server tcp_client 
