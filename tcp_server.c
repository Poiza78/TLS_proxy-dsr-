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

static int substitute(json_t* params, char* expression)
{
	char buf[BUF_SIZE], var[BUF_SIZE];
	const char  *value_string;
	int i=0, j=0, k;
	memset(buf,0, sizeof(buf));
	while (expression[i] != '\0'){
		if (isalpha(expression[i])){
			k = 0;
			do { var[k++] = expression[i++];
			} while (isalpha(expression[i]));
			var[k] = '\0';
			if (value_string =
				json_string_value(json_object_get(params,var))){
				strcat(buf, value_string);
				j += strlen(value_string);
			} else return 1; // no required param
		} else buf[j++] = expression[i++];
	}
	buf[j]='\0';
	strcpy(expression,buf);
	return 0;
}
static void exp_to_rpn(char* expression)
{
	//shunting_yard
	char rpn[BUF_SIZE];
	int token, i=0, j=0;
	while (expression[i] != '\0'){
		switch (expression[i]){
		case '+':
			while (stack[top] == '+'
			     ||stack[top] == '*'){
				rpn[j++] = pop();
				rpn[j++] = ' ';
			}
		case '*':
		case '(':
			push(expression[i++]);
			break;
		case ')':
			while (stack[top] != '('){
				rpn[j++] = pop();
				rpn[j++] = ' ';
			}
			pop(); // remove (
			break;
		case ' ':
			break;
		default:
			do { rpn[j++] = expression[i++];
			} while (isdigit(expression[i])); // +minus
			rpn[j++] = ' ';
		}
	}
	while (top+1){
		rpn[j++] = pop();
		rpn[j++] = ' ';
	}
	rpn[j] = '\0';
	strcpy(expression,rpn);
}
static int calculate(char* rpn)
{
	int n1, n2, i = 0;
	char* token;
	token = strtok(rpn, " ");
	do{
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
	} while (token = strtok(NULL, " "));
	return pop();
}
/*
static void json_error(const char* err_msg, int* code, int code_value)
{
	fprintf(stderr, "error: can't read from socket");
	json_decref(request);
	*code = code_value;
}
*/

static void process_server(int connection_sock)
{
	int code;
	json_t *request, *params, *expressions, *response, *results;
	json_error_t error;
	results = json_array();

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
	for (int i=0; i < json_array_size(expressions) && !code ; ++i){
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
		if (substitute(params, tmp)){
			code = 1;
			sprintf(buf, "%s = %s", exp_string,"not enough params");
		} else {
			exp_to_rpn(tmp);
			sprintf(buf,"%s = %d", exp_string, calculate(tmp));
		}
		json_array_append_new(results, json_string(buf));
	}
	request_error:
	json_decref(request);

	//forming of the response
	response = json_object();
	json_object_set_new(response,"code", json_integer(code));
	if (code < 2)// if results exist
		json_object_set(response,"results", results);
	if (json_dumpfd(response,connection_sock,JSON_INDENT(1)) < 0){
		fprintf(stderr, "error: can't write to socket\n");
		json_decref(response);
		break;
	}
	json_array_clear(results);
	json_decref(response);
} // while(1)
	json_decref(results);
}
