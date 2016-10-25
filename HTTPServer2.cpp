//============================================================================
// Name        : HTTPServer.cpp
// Author      : Liu Qi
// Version     :
// Copyright   :
// Description : An HTTPServer for high concurrency and no third party library
//============================================================================

//ab -n 50000 -c 1500 http://localhost:3000/

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
#include <pthread.h>
#include <iostream>
#include <sched.h>
#define BUFF_SIZE 500 //reading buffer size
#define BACKLOG 10000 //backlog size
#define CLIENTNUM 1
#define MAX_BUFFERS (BACKLOG + CLIENTNUM)
#define CPUNUM 8
using namespace std;

int epollfd;
int epollthread;
int sock;
pthread_mutex_t mutexsum1;
pthread_mutex_t mutexsum2;

void error_msg(const char* msg, bool halt_flag) {
	perror(msg);
	if (halt_flag)
		exit(1);
}

//listening socket
int create_server_socket(bool non_blocking) {

	const int port = 3000;

	struct sockaddr_in server_addr;

	//create, bind, listen
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

	/* listen */
	printf("Listening for requests on port %i...\n", port);
	if (listen(sock, BACKLOG) < 0)
		error_msg("Problem with listen call", true);
	return sock;
}

//print who are connecting
void announce_client(struct in_addr* addr) {
	char buffer[BUFF_SIZE + 1];
	inet_ntop(AF_INET, addr, buffer, sizeof(buffer));
	printf("Client connected from %s...\n", buffer);
}

void generate_echo_response(char request[], char response[]) {
	strcpy(response, "HTTP/1.0 200 OK\n");
	strcat(response, "Content-Length:37\n");
	strcat(response, "Connection: close\n");
	strcat(response, "Content-Type: text/html\n\n");
	strcat(response, "<html><body>Hello World</body></html>");
}

void *handle_request(void * epoll_id) {
	long id = (long) epoll_id;
	char buffer[BUFF_SIZE + 1];
	struct epoll_event event, event_buffers[MAX_BUFFERS];
	struct sockaddr_in client_addr;
	event.events = EPOLLIN | EPOLLET;
	//connections + requests
	while (true) {
		if (pthread_mutex_trylock(&mutexsum1) == 0) {
			int n = epoll_wait(epollfd, event_buffers, MAX_BUFFERS, -1);
			pthread_mutex_unlock(&mutexsum1);
			if (n < 0)
				error_msg("Problem with epoll_wait call", true);

			int i;
			for (i = 0; i < n; i++) {
				//listener?
				while (true) {
					socklen_t len = sizeof(client_addr);
					int client = accept(sock, (struct sockaddr *) &client_addr,
							&len);

					//no client?
					if (client < 0 && (EAGAIN == errno || EWOULDBLOCK == errno))
						break;

					//client
					fcntl(client, F_SETFL, O_NONBLOCK); //non-blocking

					event.data.fd = client;
					if (epoll_ctl(epollthread, EPOLL_CTL_ADD, client, &event)
							< 0)
						error_msg("Problem with epoll_ctl ADD call", false);

					//announce_client(&client_addr.sin_addr);
				}

			}
		} else if (pthread_mutex_trylock(&mutexsum2) == 0) {
			int n = epoll_wait(epollthread, event_buffers, MAX_BUFFERS, -1);
			pthread_mutex_unlock(&mutexsum2);
			if (n < 0)
				error_msg("Problem with epoll_wait call", true);

			int i;
			for (i = 0; i < n; i++) {

				bzero(buffer, sizeof(buffer));
				int bytes_read = recv(event_buffers[i].data.fd, buffer,
						sizeof(buffer), 0);

				char response[800];
				bzero(response, sizeof(response));
				generate_echo_response(buffer, response);
				int bytes_written = send(event_buffers[i].data.fd, response,
						strlen(response), 0);
				if (bytes_written < 0)
					error_msg("Problem with send call", false);
				close(event_buffers[i].data.fd);

			}
		} else {
			sched_yield();
		}
	}

	return 0;
}

int main() {
	char buffer[BUFF_SIZE + 1];
//epoll structures
	struct epoll_event event, event_buffers[MAX_BUFFERS];
	struct sockaddr_in client_addr;

	sock = create_server_socket(true); //non-blocking
	pthread_mutex_init(&mutexsum1, NULL);
	pthread_mutex_init(&mutexsum2, NULL);

// polling
	epollfd = epoll_create(MAX_BUFFERS); //just a hint
	epollthread = epoll_create(MAX_BUFFERS);
	if (epollfd < 0)
		error_msg("Problem with epoll_create", true);
	event.events = EPOLLIN | EPOLLET; //reading, edge-triggered
	event.data.fd = sock; //register listener
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &event) < 0)
		error_msg("Problem with epoll_ctl call", true);
	pthread_t threads[CPUNUM - 1];
	int rc;
	//fork new processes
	int j;
	for (j = 0; j < CPUNUM - 1; j++) {

		rc = pthread_create(&threads[j], NULL, handle_request, (void *) j);
		if (rc) {
			printf("ERROR; return code from pthread_create() is %d\n", rc);
			exit(-1);
		}
	}
	handle_request((void *) 1);
	return 0;
}
