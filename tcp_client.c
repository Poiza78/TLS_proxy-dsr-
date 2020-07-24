#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <jansson.h>
#include "connection.h"

#define VAR_SIZE 10

static void process_client(int sock);

int main(int argc, const char** argv)
{
	if (argc != 2){
		fprintf(stderr, "Usage: ./tcp_client port_num\n");
                exit(EXIT_FAILURE);
        }
	int server_sock;
	server_sock = make_socket(INADDR_LOOPBACK, argv[1], CLIENT);
	process_client(server_sock);
	close(server_sock);
	exit(EXIT_SUCCESS);
}

static void set(char* line, json_t* params)
{
	char var[VAR_SIZE];
	int i=0;
	while (isalpha(line[i+4])){
		var[i] = line[i+4];
		i++;
	}
	var[i] = '\0';
	char* value = strchr(line, '=');
	if (value && atoi(++value))
		json_object_set_new(params, var, json_string(value));
	else fprintf(stderr, "Wrong value\n");
}
static void add(char* line, json_t* expressions)
{
	json_array_append_new(expressions, json_string(line+4));
}
static void calculation(int sock, json_t* params, json_t* expressions)
{
	if (!json_array_size(expressions)){
		fprintf(stderr, "Nothing to calculate\n");
		return;
	}

	json_t *request, *response, *code, *results;
	json_error_t error;

	request = json_object();
	json_object_set(request, "params", params);
	json_object_set(request, "expressions", expressions);

	if (json_dumpfd(request, sock, JSON_INDENT(1)) < 0){
		fprintf(stderr, "error: can't write to socket\n");
		goto request_error;
	}

	response = json_loadfd(sock, JSON_DISABLE_EOF_CHECK, &error);
	if (!response){
		fprintf(stderr, "error: %s\n", error.text);
		goto request_error;
	}
	if (!json_is_object(response)){
		fprintf(stderr, "error: response isn't an object\n");
		goto response_error;
	}
	code = json_object_get(response, "code");
	if (json_integer_value(code) > 1){
		fprintf(stderr,"error code: %d\n", json_integer_value(code));
		goto response_error;
	}
	results = json_object_get(response, "results");
	if (!json_is_array(results)){
		fprintf(stderr, "error: results isn't an array\n");
		goto response_error;
	}
	for (int i=0; i< json_array_size(results); ++i){
		json_t *result = json_array_get(results,i);
		if (!json_is_string(result)){
			fprintf(stderr, "error: result isn't a string\n");
			goto response_error;
		}
		printf("%s\n", json_string_value(result));
	}
	json_array_clear(expressions);
	response_error:
	json_decref(response);
	request_error:
	json_decref(request);
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
		case WRONG:
			fprintf(stderr, "unknown command\n");
			break;
		case EXIT:
			goto exit;	// goto instead of break because of 
					// nested switch
		}
		free(line); // must have inside each iteration
	}
	exit:
	free(line);
	json_decref(params);
	json_decref(expressions);
}
