#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include "common.h"


void *threadReceiveMessage(void *arg) {
	int sock = *((int *)arg);
	
	int valread;
	char buffer[MAX_BYTES] = { 0 };

	while (true) {
		// read message from server
		valread = read(sock, buffer, MAX_BYTES);
		if(valread == 0) {
			exit(0);
		}
		printf("%s", buffer);
		memset(buffer, 0, sizeof(buffer));
	}
	
	
	int* returnMessage;
	return returnMessage;
}

int main(int argc, char const* argv[])
{

	Parameters *p = malloc(sizeof(Parameters));
	if(!initProgram(p, true, argc, argv)) {
		return 1;
	}
	int protocol =AF_INET;

	int sock = 0;

	struct sockaddr_storage addServerStorage;
  	memset(&addServerStorage, 0, sizeof(addServerStorage));
	int servSize;
	struct sockaddr_in *serverAddress = (struct sockaddr_in *) &addServerStorage;
	serverAddress->sin_family = protocol;
	serverAddress->sin_addr.s_addr = INADDR_ANY;
	serverAddress->sin_port = htons(p->port);
	if(inet_pton(AF_INET, p->ip, &serverAddress->sin_addr) <= 0) {
		printf("\nInvalid address/ Address not supported \n");
		return -1;
	}
	servSize = sizeof(*serverAddress);
	struct sockaddr *serv_addr = (struct sockaddr *) &addServerStorage;


	// Initialize socket
	if ((sock = socket(protocol, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		printf("\n Socket creation error \n");
		return -1;
	}

	// Try to connect to the server
	if (connect(sock, serv_addr, servSize) < 0) {
		printf("\nConnection Failed \n");
		return -1;
	}


	// open thread to keep waiting for messages
	pthread_t thread;
	pthread_create(&thread, NULL, threadReceiveMessage, (void *)&sock);

	char *message = REQ_ADD;
	int messageSize = sizeof(message);
	send(sock, message, messageSize, 0);

	while(true) {
		size_t bufsize;
		char *command = malloc(bufsize * sizeof(char));
		getline(&command, &bufsize, stdin);

		// strip special characters from message
		int messageSize;
		stripUnwantedChars(command, &messageSize);

		// TODO: convert command to message

		// send message to server
		send(sock, command, messageSize, 0);
		free(command);
	}
	return 0;
}

