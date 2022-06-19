#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include "common.h"

#define MAX_TOKENS 4

#define debug true

int thisId = -1;
bool idDefined = false;

bool equipments[MAX_EQUIPMENTS];


void _handleError(char **tokens, int size) {
	char* errorType = tokens[1];
	if(strcmp(errorType, ERR_EQUIPMENT_NOT_FOUND) == 0) { 
		printf("Equipment not found\n");
	} else if(strcmp(errorType, ERR_SOURCE_EQUIPMENT_NOT_FOUND) == 0) { 
		printf("Source equipment not found\n");
	} else if(strcmp(errorType, ERR_TARGET_EQUIPMENT_NOT_FOUND) == 0) { 
		printf("Target equipment not found\n");
	} else if(strcmp(errorType, ERR_EQUIPMENT_LIMIT_EXCEEDED) == 0) { 
		printf("Equipment limit exceeded\n");
	}
}

void _handleEquipmenetAdded(char **tokens, int size) {
	int eqId = atoi(tokens[0]);
	
	if(idDefined) {
		printf("Equipment %s added\n", tokens[0]);
	}else{
		idDefined = true;
		printf("New ID: %s\n", tokens[0]);
		thisId = eqId;
	}
	equipments[eqId] = true;
}

void _handleCurrentEquipmentList(char **tokens, int size) {
	char **ids = malloc(sizeof(char *) * MAX_TOKENS);
	int idCount; split(tokens[0], ids, &idCount, ",");
	
	for(int i = 0; i < idCount; i++) {
		int id = atoi(ids[i]);
		equipments[id] = true;
		if(debug) printf("(debug) ceq: %d\n", id);
	}
}

void _handleServerMessage(char *message) {
	if(debug) {
		printf("(debug) Message Received: %s\n", message);
	}

	char **tokens = malloc(sizeof(char *) * MAX_TOKENS);
	int tc; split(message, tokens, &tc, " ");

	int subtSize = tc - 1;
	char **subtokens = malloc(sizeof(char *) * subtSize);
	for(int i = 1; i < subtSize + 1; i++) {
		subtokens[i - 1] = tokens[i];
	}

	char* commandType = tokens[0];
	if(strcmp(commandType, RES_ADD) == 0) {
		_handleEquipmenetAdded(subtokens, subtSize);
	} else if(strcmp(commandType, RES_LIST) == 0) {
		_handleCurrentEquipmentList(subtokens, subtSize);
	} else if(strcmp(commandType, ERROR) == 0) {
		_handleError(subtokens, subtSize);
	}
}

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

		char **messages = malloc(sizeof(char *) * MAX_TOKENS);
		int messageCount; split(buffer, messages, &messageCount, "\n");
		for(int i = 0; i < messageCount; i++) {
			_handleServerMessage(messages[i]);
		}
		memset(buffer, 0, sizeof(buffer));
		free(messages);
	}
	
	
	int* returnMessage;
	return returnMessage;
}

char* _getMessage(char* command, size_t commandSize) {

	return command;
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

		char *message = _getMessage(command, bufsize);

		// strip special characters from message
		int messageSize;
		stripUnwantedChars(message, &messageSize);

		// TODO: convert command to message

		// send message to server
		send(sock, message, messageSize, 0);
		free(message);
		free(command);
	}
	return 0;
}

