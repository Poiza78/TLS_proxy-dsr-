#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <jansson.h>
#include "connection.h"

static void process_client(int sock);

int main(int argc, const char** argv)
{
	if (argc != 2){
		fprintf(stderr, "Usage: ./tcp_client port_num\n");
                exit(EXIT_FAILURE);
        }
	int client_sock;
	client_sock = make_socket(INADDR_LOOPBACK, argv[1], CLIENT);
	process_client(client_sock);
	close(client_sock);
	exit(EXIT_SUCCESS);
}

static void set(char* line, json_t* params)
{
	char var[2];
	var[0] = *(line+4);
	var[1] = '\0';
	char* value = (strchr(line, '=')+1);
	json_object_set_new(params, var, json_integer(atoi(value)));
}
static void add(char* line, json_t* expressions)
{
	json_array_append_new(expressions, json_string(line+4));
}
static void calculation(int sock, json_t* params, json_t* expressions)
{
	json_t *request, *response, *code, *results;
	json_error_t error;
	request = json_object();

	json_object_set(request, "params", params);
	json_object_set(request, "expressions", expressions);
	if (json_dumpfd(request, sock, JSON_INDENT(1)) < 0){
		fprintf(stderr, "error: can't write to socket\n");
		json_decref(request);
		return;
	}
	json_decref(request);
	json_array_clear(expressions);

	response = json_loadfd(sock, JSON_DISABLE_EOF_CHECK, &error);
	if (!response){
		fprintf(stderr, "error: %s\n", error.text);
		return;
	}
	if (!json_is_object(response)){
		fprintf(stderr, "error: response isn't an object\n");
		json_decref(response);
		return;
	}
	code = json_object_get(response, "code");	
	if (json_integer_value(code)){
		fprintf(stderr,"error code: %d\n", json_integer_value(code));
		json_decref(response);
		return;
	}
	results = json_object_get(response, "results");
	if (!json_is_array(results)){
		fprintf(stderr, "error: results isn't an array\n");
		json_decref(response);
		return;
	}
	for (int i=0; i< json_array_size(results); ++i){
		json_t *result = json_array_get(results,i);
		if (!json_is_string(result)){
			fprintf(stderr, "error: result isn't a string\n");
			json_decref(response);
			return;
		}
		printf("%s\n", json_string_value(result));
	}
	json_decref(response);
}

enum {
WRONG = -1,
EXIT,
SET,
ADD,
CALC
};

static int identify(const char* line)
{
	if (!strncmp(line, "exit", 4))
		return EXIT;
	if (!strncmp(line, "set ", 4))
		return SET;
	if (!strncmp(line, "add ", 4))
		return ADD;
	if (!strncmp(line, "calculate", 9))
		return CALC;
	return WRONG;
}
static void process_client(int sock)
{
	printf("\nYou can use: add, set, calculate, exit\n\n");
	char* line;
	int type;

	json_t *params, *expressions;
	params = json_object();
	expressions = json_array();

	while (1){
		line = readline(">");
		if (!line)
			continue;
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
			free(line);
			json_decref(params);
			json_decref(expressions);
			return;
		case WRONG:
			fprintf(stderr, "unknown command\n");
			continue;
		}
		free(line);
	}
}
