#ifndef CALCULATION
#define CALCULATION

#include <jansson.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>

#define BUF_SIZE 1024

static void substitute(json_t* params, char* expression);
static void exp_to_rpn(char* expression);
static void calculate(char* rpn);

void process(int connection_sock){
	int code = 0;
	json_t *request, *params, *expressions, *answer;
	json_error_t error;
	request = json_loadfd(connection_sock, 0, &error);
	if (!request){
		fprintf(stderr, "error: %s\n", error.text);
		exit(EXIT_FAILURE);
	}
	if (!json_is_object(request)){
		fprintf(stderr, "error: request isn't an object\n");
		json_decref(request);
		exit(EXIT_FAILURE);
	}
	params = json_object_get(request, "params");
	if (!json_is_object(request)){
		fprintf(stderr, "error: params isn't an object\n");
		code = 2;
		exit(EXIT_FAILURE);
	}
	expressions = json_object_get(request, "expressions");
	if (!json_is_array(expressions)){
		fprintf(stderr, "error: expressions isn't an array\n");
		exit(EXIT_FAILURE);
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
		calculate(tmp);
		sprintf(buf,"%s = %s", exp_string, tmp);
		printf("%s\n",buf);
	}
	
}

int top, stack[BUF_SIZE];

static void push(int x){
	if (top != BUF_SIZE)
		stack[top++] = x;
}
static int pop(void){
	return stack[--top];
}

static void substitute(json_t* params, char* expression){
	const char *var;
	json_t *value;
	json_object_foreach(params, var, value){
		for (int i=0; i<strlen(expression); i++){
			if (expression[i] == *var)
				expression[i] = json_integer_value(value) + '0'; 
		}
	}
}
static void exp_to_rpn(char* expression){
	char rpn[BUF_SIZE];
	int c, i=0, j=0;
	while ((c = expression[i++]) != '\0'){
		if (isdigit(c)){
			rpn[j++] = c;
			rpn[j++] = ' ';
		}
		if (c == '(' || c == '*')
			push(c);
		if (c == ')'){
			while (stack[top-1] != '(')
				rpn[j++] = pop();
			pop(); // remove (
		}
		if (c == '+'){
			while (stack[top-1] == '+' 
				|| stack[top-1] == '*')
				rpn[j++] = pop();
			push(c);
		}	
			
	}
	while (top)
		rpn[j++] = pop();
	strcpy(expression,rpn);
}
static void calculate(char* rpn){
	int token,  i = 0;
	char answer[BUF_SIZE];
	while ((token = rpn[i++]) != '\0'){
		switch (token){
		case '+':
			push(pop() + pop());
			break;
		case '*':
			push(pop() * pop());
			break;
		default:
			if (isdigit(token)) push(token - '0');
	}	}
	sprintf(answer,"%d",pop());
	strcpy(rpn,answer);
}
#endif
