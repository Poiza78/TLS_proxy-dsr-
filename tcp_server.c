#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <jansson.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "connection.h"

#define BUF_SIZE 1024

static void process_server(int connection_sock);

int main(int argc, const char** argv){
	if (argc != 2){
		fprintf(stderr, "Usage: ./tcp_server port_num\n");
		exit(EXIT_FAILURE);
	}
	int server_sock, client_sock;
	server_sock = make_socket(INADDR_ANY, argv[1], SERVER);
	if (listen(server_sock, SOMAXCONN) < 0)
		error("listen");
	while(1){
		client_sock = accept(server_sock, 0,0);
		if (client_sock < 0){
			perror("accept");
			continue;
		}
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

static int get_number(char **expression)
{
	return strtol(*expression, expression, 10);
}
static int get_param_value(char **expression, json_t *params)
{
	char var[BUF_SIZE];
	int k = 0;
	char *token = *expression;

	do { var[k++] = *token++;
	} while (isalpha(*token));
	var[k] = '\0';

	*expression = token;

	return json_integer_value(json_object_get(params,var));
}

enum {
OPERATOR,
CL_BR,
OP_BR,
NUMBER
};
static int exp_to_rpn(char* expression, json_t *params)
{
	//shunting_yard
	char rpn[BUF_SIZE], value_string[BUF_SIZE];
	char *token;
	int value;
	int j=0, prev = -1;

	token = expression;
	memset(rpn, 0, sizeof rpn);
	top = -1;
	while (*token != '\0'){
		switch (*token){
		case '+':
			if (OPERATOR == prev || OP_BR == prev) return 3;
			while ( top >= 0
			&& ('+' == stack[top] || '*' == stack[top])
			&& '(' != stack[top]){
				rpn[j++] = pop();
				rpn[j++] = ' ';
			}
			push(*token++);
			prev = OPERATOR;
			break;
		case '*':
			if (OPERATOR == prev || OP_BR == prev) return 3;
			while ( top >= 0
			&&  '*' == stack[top]
			&& '(' != stack[top]){
				rpn[j++] = pop();
				rpn[j++] = ' ';
			}
			prev = OPERATOR;
		case '(':
			if (NUMBER == prev || CL_BR == prev) return 3;
			push(*token++);
			prev = OP_BR;
			break;
		case ')':
			if (OPERATOR == prev || OP_BR == prev) return 3;
			while ('(' != stack[top]){
				rpn[j++] = pop();
				rpn[j++] = ' ';
				if ( top < 0) return 4;
			}
			if ('(' == stack[top]) pop();
			prev = CL_BR;
		case ' ':
			token++;
			break;
		default:
			if (NUMBER == prev || CL_BR == prev) return 3;
			if (isdigit(*token) || '-' == *token)
				value = get_number(&token);
			else if (isalpha(*token)){
				value = get_param_value(&token, params);
				if (!value) return 5;
			}
			sprintf(value_string, "%d", value);
			strcat(rpn, value_string);
			j += strlen(value_string);
			rpn[j++] = ' ';
			prev = NUMBER;
		}
	}
	while (top >= 0){
		rpn[j++] = pop();
		if ( '(' == rpn[j-1]
		  || ')' == rpn[j-1])
			return 4;
		rpn[j++] = ' ';
	}
	rpn[j] = '\0';
	strcpy(expression,rpn);
	return 0;
}
static int calculate(char* rpn)
{
	int n1, n2;
	char* token;
	token = strtok(rpn, " ");
	do {
		if (!strcmp(token,"+")){
			n1 = pop();
			n2 = pop();
			push(n1 + n2);
			continue;
		}
		if (!strcmp(token,"*")){
			n1 = pop();
			n2 = pop();
			push(n1 * n2);
			continue;
		}
		push(atoi(token));
	} while ((token = strtok(NULL, " ")));
	return pop();
}

static void process_server(int connection_sock)
{
	int code;
	json_t *request, *params, *expressions, *response, *results;
	json_error_t error;
	results = json_array();
	response = json_object();

while (1){
	code = 0;
	request = json_loadfd(connection_sock, JSON_DISABLE_EOF_CHECK, &error);
	if (!request){
		fprintf(stderr, "error: %s\n", error.text);
		break;
	}
	if (!json_is_object(request)){
		fprintf(stderr, "error: request isn't an object\n");
		code = 2;
		goto request_error;
	}
	params = json_object_get(request, "params");
	if (!json_is_object(params)){
		fprintf(stderr, "error: params isn't an object\n");
		code = 2;
		goto request_error;
	}
	expressions = json_object_get(request, "expressions");
	if (!json_is_array(expressions)){
		fprintf(stderr, "error: expressions isn't an array\n");
		code = 2;
		goto request_error;
	}
	for (int i=0; i < json_array_size(expressions); ++i){
		char buf[BUF_SIZE], tmp[BUF_SIZE];
		const char* exp_string;
		json_t *exp = json_array_get(expressions,i);
		if (!json_is_string(exp)){
			fprintf(stderr,"error: expression isn't a string\n");
			code = 2;
			goto request_error;
		}
		exp_string = json_string_value(exp);
		strcpy(tmp,exp_string);
		if ((code = exp_to_rpn(tmp, params)))
			goto request_error;
		sprintf(buf,"%s = %d", exp_string, calculate(tmp));
		json_array_append_new(results, json_string(buf));
	}
	request_error:
	json_decref(request);

	//forming of the response
	json_object_set_new(response,"code", json_integer(code));
	if (json_array_size(results))
		json_object_set(response,"results", results);
	if (json_dumpfd(response,connection_sock,0) < 0){
		fprintf(stderr, "error: can't write to socket\n");
		break;
	}

	json_array_clear(results);
	json_object_clear(response);
} // while(1)
	json_decref(results);
	json_decref(response);
}
