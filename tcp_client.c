#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <jansson.h>
#include "connection.c"

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
	char* value = (strchr(line, '=')+1);
	json_object_set_new(params, var, json_integer(atoi(value)));
}
static void add(char* line, json_t* expressions)
{
	json_array_append_new(expressions, json_string(line+4));
}
static void calculation(int sock, json_t* params, json_t* expressions)
{
	json_t *request, *answer, *code, *results;
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
	code = json_object_get(answer, "code");	
	if (json_integer_value(code)){
		fprintf(stderr,"error code: %d\n", json_integer_value(code));
		return;
	}
	results = json_object_get(answer, "results");
	for (int i=0; i< json_array_size(results); ++i){
		json_t *result = json_array_get(results,i);
		printf("%s\n", json_string_value(result));
		json_decref(result);
	}
	//printf("%s\n", json_dumps(request, 0));
	//printf("%s\n", json_dumps(answer, 0));
	json_decref(code);
	json_decref(results);
	json_decref(request);
	json_decref(answer);
	json_array_clear(expressions);
}
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
