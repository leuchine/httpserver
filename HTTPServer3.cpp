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

#include <iostream>

#define BUFF_SIZE 10000 //reading buffer size
#define BACKLOG 11000 //backlog size
#define CLIENTNUM 1
#define MAX_BUFFERS (BACKLOG + CLIENTNUM)
#define CPUNUM 8
using namespace std;

void error_msg(const char* msg, bool halt_flag) {
	perror(msg);
	if (halt_flag)
		exit(1);
}

//listening socket
int create_server_socket(bool non_blocking) {

	const int port = 4001;

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
	strcpy(response, "HTTP/1.1 200 OK\n");
	strcat(response, "Content-Length:37\n");
	strcat(response, "Connection: keep-alive\n");
	strcat(response, "Content-Type: text/html\n\n");
	strcat(response, "<html><body>Hello World</body></html>");
}

void *handle_request(void * epoll_id) {
	int epollid;
	epollid = *(int*) epoll_id;
	printf("Hello World! It's me, epoll #%d!\n", epollid);
	char response[800];
	char buffer[BUFF_SIZE + 1];
	struct epoll_event event, event_buffers[MAX_BUFFERS];
	while (true) {
		int n = epoll_wait(epollid, event_buffers, MAX_BUFFERS, -1);
		if (n < 0)
			error_msg("Problem with epoll_wait call", true);
		int i;
		for (i = 0; i < n; i++) {
			bzero(buffer, sizeof(buffer));
			int bytes_read = recv(event_buffers[i].data.fd, buffer,
					sizeof(buffer), 0);
			if (bytes_read <= 0) {
				close(event_buffers[i].data.fd);
				continue;
			}

			bzero(response, sizeof(response));
			generate_echo_response(buffer, response);

			int bytes_written = send(event_buffers[i].data.fd, response,
					strlen(response), 0);

			if (bytes_written < 0)
				error_msg("Problem with send call", false);



		}
	}
	return 0;
}

int main() {
	char buffer[BUFF_SIZE + 1];
//epoll structures
	struct epoll_event event, event_buffers[MAX_BUFFERS];

	struct sockaddr_in client_addr;
	int sock = create_server_socket(true); //non-blocking

//create epoll for threads
	int epollthreadfds[CPUNUM - 1];
	int j;
	for (j = 0; j < CPUNUM - 1; j++) {
		epollthreadfds[j] = epoll_create(MAX_BUFFERS);
	}
	pthread_t threads[CPUNUM - 1];
	int rc;
	for (j = 0; j < CPUNUM - 1; j++) {

		rc = pthread_create(&threads[j], NULL, handle_request,
				(void *) &epollthreadfds[j]);
		if (rc) {
			printf("ERROR; return code from pthread_create() is %d\n", rc);
			exit(-1);
		}
	}

// polling
	int epollfd = epoll_create(MAX_BUFFERS); //just a hint
	if (epollfd < 0)
		error_msg("Problem with epoll_create", true);

	event.events = EPOLLIN | EPOLLET; //reading, edge-triggered
	event.data.fd = sock; //register listener
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &event) < 0)
		error_msg("Problem with epoll_ctl call", true);
//connections + requests
	int count = 0;
	while (true) {
		int n = epoll_wait(epollfd, event_buffers, MAX_BUFFERS, -1);
		if (n < 0)
			error_msg("Problem with epoll_wait call", true);

		int i;
		for (i = 0; i < n; i++) {
			//listener?
			if (event_buffers[i].data.fd == sock) {
				while (true) {
					socklen_t len = sizeof(client_addr);
					int client = accept(sock, (struct sockaddr *) &client_addr,
							&len);

					//no client?
					if (client < 0 && (EAGAIN == errno || EWOULDBLOCK == errno))
						break;
					//client
					fcntl(client, F_SETFL, O_NONBLOCK); //non-blocking
					event.events = EPOLLIN | EPOLLET;
					event.data.fd = client;

					if (epoll_ctl(epollthreadfds[count], EPOLL_CTL_ADD, client, &event) < 0)
						error_msg("Problem with epoll_ctl ADD call", false);
					count = (count + 1) % (CPUNUM - 1);
				}
			}
		}
	}
	return 0;
}
