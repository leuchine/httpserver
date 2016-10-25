#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <map>

using namespace std;

//default response
char default_response[] = "HTTP/1.1 200 OK\nContent-Length:37\n"
		"Connection: keep-alive\n\n<html><body>Hello World</body></html>";
//parameter response
char parameter_response1[] =
		"HTTP/1.1 200 OK\nContent-Length:%d\nConnection: keep-alive\n\n";
char parameter_response2[] = "<html><body>Hello World:%s</body></html>";
//head request response
char head_response[] =
		"HTTP/1.1 200 OK\nContent-Length:0\nConnection: keep-alive\n\n";

//options request response
char options_response[] =
		"HTTP/1.1 200 OK\nAllow: GET,HEAD,POST,OPTIONS,PUT,CONNECT\nConnection: keep-alive\n\n";

//connect request response
char connect_response[] = "HTTP/1.1 200 OK\nConnection: keep-alive\n\n";

//request by apache AB
char default_request[] = "GET / HTTP/1.0\nHost: localhost:4001\n"
		"User-Agent: ApacheBench/2.3\nAccept: */*\n\n";
//404 response
char not_found[] = "HTTP/1.1 404 Not Found\n";

//for processing requests
char* line;
char* word;
char* saveptr;
char* saveptrline;
char formatbuffer1[400];
char formatbuffer2[400];

//for PUT method
map<string, string> resources;

//format final HTML page
void get_HTML(char response[], char parameter[]) {
	if (parameter == NULL)
		strcpy(response, default_response);
	//response with parameter
	else {
		sprintf(formatbuffer1, parameter_response2, parameter);
		sprintf(formatbuffer2, parameter_response1, strlen(formatbuffer1));
		strcpy(response, formatbuffer2);
		strcat(response, formatbuffer1);
	}
}

//get request
void handle_get(char response[]) {
	//path and parameter
	word = strtok_r(NULL, " ", &saveptrline);
	//get path and parameter
	char *saveptrlineword;
	word = strtok_r(word, "?", &saveptrlineword);
	char* parameter = strtok_r(NULL, "?", &saveptrlineword);
	if (word != NULL && strcmp(word, "/") == 0) {
		get_HTML(response, parameter);
	} else if (word != NULL && resources.count(word)>0){
	    char resource[400];
	    strcpy(resource,resources[word].c_str());
	    get_HTML(response, resource);
	}
	else {
		strcpy(response, not_found);
	}
}

//post request
void handle_post(char response[]) {
	//path
	word = strtok_r(NULL, " ", &saveptrline);
	//get parameter
	char * parameter = line;
	while (line != NULL) {
		parameter = line;
		line = strtok_r(NULL, "\n", &saveptr);
	}
	if (word != NULL && strcmp(word, "/") == 0) {
		get_HTML(response, parameter);
	} else {
		strcpy(response, not_found);
	}
}

//head request
void handle_head(char response[]) {
	strcpy(response, head_response);
}

//options request
void handle_options(char response[]) {
	strcpy(response, options_response);
}

//connect request
void handle_connect(char response[]) {
	strcpy(response, connect_response);
}

//connect request
void get_error(char response[]) {
	strcpy(response, not_found);
}

//put request
void handle_put(char response[]) {
	//path
	word = strtok_r(NULL, " ", &saveptrline);
	//get parameter
	char * parameter = line;
	while (line != NULL) {
		parameter = line;
		line = strtok_r(NULL, "\n", &saveptr);
	}
	if (word != NULL && parameter != NULL && strcmp(word, "/") != 0 ) {
		string key=word;
		string value=parameter;
		resources[key]=value;
	}
	strcpy(response, default_response);
}

void generate_echo_response(char request[], char response[]) {
	//split request based on \r\n
	line = strtok_r(request, "\r\n", &saveptr);
	//for apache AB test
	if (line != NULL && strcmp(line, "GET / HTTP/1.0") == 0) {
		strcpy(response, default_response);
	} else if (line != NULL) {
		//Method
		word = strtok_r(request, " ", &saveptrline);
		//GET request
		if (word != NULL && strcmp(word, "GET") == 0) {
			handle_get(response);
		}
		//POST request
		else if (word != NULL && strcmp(word, "POST") == 0) {
			handle_post(response);
		}
		//HEAD request
		else if (word != NULL && strcmp(word, "HEAD") == 0) {
			handle_head(response);
		}
		//OPTIONS request
		else if (word != NULL && strcmp(word, "OPTIONS") == 0) {
			handle_options(response);
		}
		//CONNECT request
		else if (word != NULL && strcmp(word, "CONNECT") == 0) {
			handle_connect(response);

		}
		//PUT request
		else if (word != NULL && strcmp(word, "PUT") == 0) {
			handle_put(response);
		} else {
			get_error(response);
		}
	}
}

//int main() {
//	char response[400];
//	char request[] = "PUT /\nabc";
//	generate_echo_response(request, response);
//}
