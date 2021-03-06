//============================================================================
// Name        : HTTPServer.cpp
// Author      : Liu Qi
// Version     :
// Copyright   :
// Description : An HTTPServer for high concurrency and no third party library
//============================================================================

#include <arpa/inet.h>
#include <asm-generic/errno.h>
#include <asm-generic/errno-base.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sched.h>
#include <iostream>
#include "http.h"

#define BUFF_SIZE 10000 //reading buffer size
#define BACKLOG 10000 //backlog size
#define MAX_BUFFERS (BACKLOG + BUFF_SIZE) //epoll buffer size
/*thread number. Set it less than CPU number because thread context switch waste time
 * my cpu has 8 cores but it has best performance with 1 threads.
 */
#define CPUNUM 1

using namespace std;

//listening socket
int sock;
//event epoll
int epollfd;
int epollfdread;

//print error message
void error_msg(const char* msg, bool halt_flag) {
	perror(msg);
	if (halt_flag)
		exit(1);
}

//listening socket
int create_server_socket(bool non_blocking) {
	struct sockaddr_in server_addr;
	int port = 4001; //port number

	//create
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (socket < 0)
		error_msg("Problem with socket call", true);

	// non-blocking?
	if (non_blocking)
		fcntl(sock, F_SETFL, O_NONBLOCK);

	//bind
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(port);
	if (bind(sock, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0)
		error_msg("Problem with bind call", true);

	//listen
	printf("Listening for requests on port %i...\n", port);
	if (listen(sock, BACKLOG) < 0)
		error_msg("Problem with listen call", true);
	return sock;
}

void *handle_request(void * epoll_id) {
	//read buffer
	char buffer[BUFF_SIZE + 1];
	//epoll structures
	struct epoll_event event, event_buffers[MAX_BUFFERS];
	event.events = EPOLLIN | EPOLLET;

	struct sockaddr_in client_addr;
	socklen_t len = sizeof(client_addr);

	char response[400];
	int n, i, bytes_read, client;
	while (true) {
		//read event from epoll
		n = epoll_wait(epollfd, event_buffers, MAX_BUFFERS, 0);
		if (n > 0) {
			while (true) {
				//accept connection request
				client = accept(sock, (struct sockaddr *) &client_addr, &len);
				//no client?
				if (client < 0 && (EAGAIN == errno || EWOULDBLOCK == errno))
					break;
				//register connection to epoll
				fcntl(client, F_SETFL, O_NONBLOCK); //non-blocking
				event.data.fd = client;
				if (epoll_ctl(epollfdread, EPOLL_CTL_ADD, client, &event) < 0)
					error_msg("Problem with epoll_ctl ADD call", false);
			}
		}
		n = epoll_wait(epollfdread, event_buffers, MAX_BUFFERS, 0);
		for (i = 0; i < n; i++) {
			bytes_read = recv(event_buffers[i].data.fd, buffer, sizeof(buffer),
					0);
			//tackle EOF
			if (bytes_read <= 0) {
				close(event_buffers[i].data.fd);
				continue;
			}
			generate_echo_response(buffer, response);
			send(event_buffers[i].data.fd, response, strlen(response), 0);
		}
	}
	return 0;
}

int main() {
	//create global variable
	sock = create_server_socket(true); //non-blocking
	epollfd = epoll_create(MAX_BUFFERS);
	epollfdread = epoll_create(MAX_BUFFERS);
	if (epollfd < 0)
		error_msg("Problem with epoll_create", true);

	//register sock to epoll
	struct epoll_event event;
	event.events = EPOLLIN | EPOLLET; //reading, edge-triggered
	event.data.fd = sock; //register listener
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &event) < 0)
		error_msg("Problem with epoll_ctl call", true);

	//create threads
	int j, rc;
	pthread_t threads[CPUNUM - 1];
	for (j = 0; j < CPUNUM - 1; j++) {
		rc = pthread_create(&threads[j], NULL, handle_request, (void *) j);
		if (rc) {
			printf("ERROR; return code from pthread_create() is %d\n", rc);
			exit(-1);
		}
	}
	//handle connections and requests
	handle_request(NULL);

	return 0;
}
