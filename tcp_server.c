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

static int substitute(json_t* params, char* expression)
{
	char buf[BUF_SIZE], var[BUF_SIZE], value_string[BUF_SIZE];
	int i=0, j=0, k, value;
	memset(buf,0, sizeof(buf));
	while (expression[i] != '\0'){
		if (isalpha(expression[i])){
			k = 0;
			do { var[k++] = expression[i++];
			} while (isalpha(expression[i]));
			var[k] = '\0';
			value = json_integer_value(json_object_get(params,var));
			if (value){
				sprintf(value_string, "%d", value);
				strcat(buf, value_string);
				j += strlen(value_string);
			} else return 1; // no required param
		} else if (isspace(expression[i])){ // don't copy whitespaces
			i++;
		} else buf[j++] = expression[i++];
	}
	buf[j]='\0';
	strcpy(expression,buf);
	return 0;
}

static int is_right_exp(char* expression)
{
	int prev, count = (expression[0] == '('), i = 1;

	if (expression[0] != '('
	&& expression[0] != '-'
	&& !isdigit(expression[0]))
		return 0;

	while ((prev = expression[i++]) != '\0'){
		switch(expression[i]){
		case ')':
			count--;
		case '+':
		case '*':
			if (!isdigit(prev) && prev != ')')
				return 0;
			break;
		case '(':
			count++;
			if (prev !='+' && prev !='*' && prev !='(')
				return 0;
			break;
		}
	}
	return (!count);
}
static void exp_to_rpn(char* expression)
{
	//shunting_yard
	char rpn[BUF_SIZE];
	int i=0,j=0;

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
			i++;
			break;
		default:
			do { rpn[j++] = expression[i++];
			} while (isdigit(expression[i]));
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
		if (substitute(params, tmp)){
			code = 1;
			strcpy(tmp, "not enough params");
		} else if (!is_right_exp(tmp)){
			code = 1;
			strcpy(tmp, "wrong expression");
		} else {
			exp_to_rpn(tmp);
			sprintf(tmp, "%d", calculate(tmp));
		}
		sprintf(buf,"%s = %s", exp_string, tmp);
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
