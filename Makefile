CC=gcc
LIBS= -ljansson -lreadline -lssl -lcrypto

all: tcp_server tcp_client tls_server

tcp_server: tcp_server.o connection.o
	$(CC) tcp_server.o connection.o -o tcp_server $(LIBS)

tls_server: tls_server.o connection.o
	$(CC) tls_server.o connection.o -o tls_server $(LIBS)

tcp_client: tcp_client.o connection.o
	$(CC) tcp_client.o connection.o -o tcp_client $(LIBS)

connection.o: connection.c connection.h
	$(CC) -c connection.c -o connection.o 

tcp_server.o: tcp_server.c 
	$(CC) -c -g tcp_server.c -o tcp_server.o 

tls_server.o: tls_server.c 
	$(CC) -c tls_server.c -o tls_server.o 

tcp_client.o: tcp_client.c 
	$(CC) -c -g tcp_client.c -o tcp_client.o 

clean: 
	rm *.o tcp_server tcp_client tls_server
