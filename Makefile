CC = gcc
LIBS = -ljansson -lreadline -lssl -lcrypto
exe = tcp_server tcp_client tls_server

all : $(exe)

tcp_server: tcp_server.o connection.o
	$(CC) $^ -o $@ $(LIBS)

tls_server: tls_server.o connection.o
	$(CC) $^ -o $@ $(LIBS)

tcp_client: tcp_client.o connection.o
	$(CC) $^ -o $@ $(LIBS)


%.o : CFLAGS = -g
connection.o: connection.h

.PHONY : clean
clean : 
	rm *.o $(exe)
