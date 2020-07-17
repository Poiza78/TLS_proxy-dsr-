#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <jansson.h>
#include "tcp_connection.c"

static void process_client(int sock);

int main(int argc, const char** argv)
{
	if (argc != 2){
		fprintf(stderr, "Usage: ./tcp_client port_num\n");
                exit(EXIT_FAILURE);
        }
	int client_sock;
	client_sock = make_socket(argv[1], INADDR_LOOPBACK, CLIENT);
	process_client(client_sock);
	close(client_sock);
	exit(EXIT_SUCCESS);
}

enum {
WRONG = -1,
EXIT,
SET,
ADD,
CALC
};

static void set(char* line, json_t* params)
{
	char var[2];
	var[0] = *(line+4);
	var[1] = '\0';
	char* value = strchr(line, '=')+1;
	json_object_set_new(params, var, json_integer(atoi(value)));
}
static void add(char* line, json_t* expressions)
{
	json_array_append_new(expressions, json_string(line+4));
}
static void calculation(int sock, json_t* params, json_t* expressions)
{
	json_t *request, *answer;
	json_error_t error;
	request = json_object();
	json_object_set(request, "params", params);
	json_object_set(request, "expressions", expressions);

	json_dumpfd(request, sock, JSON_INDENT(1));
	answer = json_loadfd(sock, JSON_DISABLE_EOF_CHECK, &error);
	if (!answer){
		fprintf(stderr, "error: %s\n", error.text);
		return;
	}
	printf("%s\n", json_dumps(request, 0));
	printf("%s\n", json_dumps(answer, 0));
	json_decref(request);
	if (expressions)
		json_decref(expressions);
}
static int identify(const char* line)
{
	if (!strncmp(line, "exit", 4))
		return EXIT;
	if (!strncmp(line, "set", 3))
		return SET;
	if (!strncmp(line, "add", 3))
		return ADD;
	if (!strncmp(line, "calculation", 11))
		return CALC;
	return WRONG;
}
static void process_client(int sock)
{
	char* line;
	int type;

	json_t *params, *expressions;
	params = json_object();
	expressions = json_array();

	while (1){
		line = readline(">");
		add_history(line);
		type = identify(line);
		switch (type){
		case ADD:
			add(line, expressions);
			break;
		case SET:
			set(line, params);
			break;
		case CALC:
			calculation(sock, params,expressions);
			break;
		case EXIT:
			return;
		case WRONG:
			fprintf(stderr, "unknown command\n");
			break;
		}
		free(line);
	}
}

/*
static void process_client(int sock)
{
	json_t *exp, *string, *root;
	json_t *params = json_object();
	json_error_t error;	
	json_object_set(params,"x",json_integer(4));
	json_object_set(params,"y",json_integer(3));
	string = json_array();
	json_array_append_new(string, json_string("3+2"));
	json_array_append_new(string, json_string("x*y"));
	root = json_object();

	json_object_set(root,"params",params);
	json_object_set(root,"expressions",string);
	printf("%s\n", json_dumps(root,0));
	json_t* request;

	json_dumpfd(root,sock,JSON_INDENT(1));
	request = json_loadfd(sock, JSON_DISABLE_EOF_CHECK, &error);

	if (!request){
		fprintf(stderr, "error: cant read from socket and  %s\n", error.text);
		exit(EXIT_FAILURE);
	}
	printf("%s\n", json_dumps(request,0));
} */
