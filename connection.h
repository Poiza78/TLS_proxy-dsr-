#ifndef CONNECTION_H
#define CONNECTION_H

#define QUEUE_SIZE 3
#define SERVER 0
#define CLIENT 1

void error(const char* err_msg);
int make_socket(int ip, const char* port, int type);

//TODO
//SSL_CTX* init_CTX(SSL_METHOD *(*method)(void)){}
//void configure_cert(SSL_CTX* ctx, char* cert, char* key){}
//

#endif
