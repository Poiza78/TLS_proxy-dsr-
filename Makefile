CC = gcc
LDLIBS = -ljansson -lreadline -lssl -lcrypto
exe = tcp_server tcp_client tls_server tls_client

all : $(exe)

tcp_server: tcp_server.o connection.o
	$(CC) $^ -o $@ $(LDLIBS)

tls_server: tls_server.o connection.o
	$(CC) $^ -o $@ $(LDLIBS)

tcp_client: tcp_client.o connection.o
	$(CC) $^ -o $@ $(LDLIBS)

tls_client: tls_client.o connection.o
	$(CC) $^ -o $@ $(LDLIBS)

%.o : CFLAGS = -g
connection.o: connection.h

.PHONY : clean
clean : 
	rm *.o $(exe)
