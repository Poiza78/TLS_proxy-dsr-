#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <jansson.h>
#include <sys/socket.h>
#include "connection.c"

#define BUF_SIZE 1024

static void process_server(int connection_sock);

int main(int argc, const char** argv){
	if (argc != 2){
		fprintf(stderr, "Usage: ./tcp_server port_num\n");
		exit(EXIT_FAILURE);
	}
	int server_sock, client_sock;
	server_sock = make_socket(argv[1], INADDR_ANY, SERVER);
	while(1){
		client_sock = accept(server_sock, 0,0);
		if (client_sock < 0)
			error("accept");
		process_server(client_sock);
		close(client_sock); 
	}
	exit(EXIT_SUCCESS);
}

int top = -1, stack[BUF_SIZE];

static void push(int x)
{
	if (top != BUF_SIZE)
		stack[++top] = x;
}
static int pop(void)
{
	return (top+1)?stack[top--]:0;
}

static void substitute(json_t* params, char* expression)
{	
	const char *var;
	json_t *value;
	json_object_foreach(params, var, value)
		for (int i=0; i<strlen(expression); i++)
			if (expression[i] == *var)
				expression[i++] = json_integer_value(value) + '0';
}
static void exp_to_rpn(char* expression)
{
	//shunting_yard
	char rpn[BUF_SIZE];
	int token, i=0, j=0;
	while ((token = expression[i++]) != '\0')
		switch (token){
		case '+':
			while (stack[top] == '+'
			     ||stack[top] == '*')
				rpn[j++] = pop();
		case '*':
		case '(':
			push(token);
			break;
		case ')':
			while (stack[top] != '(')
				rpn[j++] = pop();
			pop(); // remove (
			break;
		default: 
			if (isdigit(token)){
				rpn[j++] = token;
				rpn[j++] = ' ';
			}
		}	
	while (top+1)
		rpn[j++] = pop();
	rpn[j] = '\0';
	strcpy(expression,rpn);
}
static int calculate(char* rpn)
{
	int i = 0, token;
	char answer[BUF_SIZE];
	while ((token = rpn[i++]) != '\0')
		switch (token){
		case '+':
			push(pop() + pop());
			break;
		case '*':
			push(pop() * pop());
			break;
		default:
			if (isdigit(token)) 
				push(token - '0');
	}	
	return pop();
}
static void process_server(int connection_sock)
{
	while (1){
	int code = 0;
	json_t *request, *params, *expressions, *answer, *results;
	json_error_t error;
	answer = json_object();
	results = json_array();
	request = json_loadfd(connection_sock, JSON_DISABLE_EOF_CHECK, &error);
	if (!request)
		return;
	if (!json_is_object(request)){
		fprintf(stderr, "error: request isn't an object\n");
		code = 2;
		return;
	}
	params = json_object_get(request, "params");
	if (!json_is_object(params)){
		fprintf(stderr, "error: params isn't an object\n");
		code = 2;
		return;
	}
	expressions = json_object_get(request, "expressions");
	if (!json_is_array(expressions)){
		fprintf(stderr, "error: expressions isn't an array\n");
		code = 2;
		return;
	}
	for (int i=0; i< json_array_size(expressions); ++i){
		char buf[BUF_SIZE], tmp[BUF_SIZE];
		const char* exp_string;
		json_t *exp = json_array_get(expressions,i);
		if (!json_is_string(exp)){
			code = 1;
			fprintf(stderr,"error: wrong expression\n");
			continue;
		}
		exp_string = json_string_value(exp);
		strcpy(tmp,exp_string);
		substitute(params, tmp);
		exp_to_rpn(tmp);
		sprintf(buf,"%s = %d", exp_string, calculate(tmp));
		json_array_append_new(results, json_string(buf));
	}
	json_object_set_new(answer,"code", json_integer(code));
	json_object_set(answer,"results", results);
	int n = json_dumpfd(answer,connection_sock,JSON_INDENT(1));
	if ( n < 0 ){
		fprintf(stderr, "error: can't write to socket");
		exit(EXIT_FAILURE);
	}
	json_decref(request);
	json_decref(results);
	}
}
