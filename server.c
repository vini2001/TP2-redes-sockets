// Server side C/C++ program to demonstrate Socket
// programming
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "common.h"
#include <arpa/inet.h>

#define MAX_TOKENS 10

// Valid sensor id range
#define SENSOR_RANGE_FROM 1
#define SENSOR_RANGE_TO 4

// Valid equipment id range
#define EQUIPMENT_RANGE_FROM 1
#define EQUIPMENT_RANGE_TO 4

#define DESTINATION_EQ_ID 1
#define DESTINATION_SOCKET_ID 2

struct threadArgs {
	int sockId;
	int threadId;
};
typedef struct threadArgs threadArgs;

// Struct to hold the data of an equipment

bool equipments[MAX_EQUIPMENTS];
int sensorCount = 0;

pthread_t threads[MAX_EQUIPMENTS];
bool busyThreads[MAX_EQUIPMENTS];
int threadSocketsMap[MAX_EQUIPMENTS];

int threadId() {
	int i;
	for(i = 0; i < MAX_EQUIPMENTS; i++) {
		if(!busyThreads[i]) {
			return i;
		}
	}
	return -1;
}

// Init the equipments without any sensors
void _initEquipments() {
	for(int i = 0; i < MAX_EQUIPMENTS; i++) {
		equipments[i] = false;
		busyThreads[i] = false;
	}
}


void _sendMessage(char* idMsg, int originEqId, int destinationId, char* payload, int destinationType) {
	char message[MAX_BYTES] = { 0 };
	sprintf(message, "%s", idMsg);

	if(strcmp(idMsg, REQ_REM) == 0 || strcmp(idMsg, REQ_INF) == 0 || strcmp(idMsg, RES_INF) == 0) {
		sprintf(message, "%s %s%d", message, originEqId < 10 ? "0" : "", originEqId);
	}

	if(strcmp(idMsg, REQ_INF) == 0 || strcmp(idMsg, RES_INF) == 0 || strcmp(idMsg, ERROR) == 0 || strcmp(idMsg, OK) == 0) {
		sprintf(message, "%s %s%d", message, destinationId < 10 ? "0" : "", destinationId);
	}

	if(strcmp(idMsg, RES_ADD) == 0 || strcmp(idMsg, RES_LIST) == 0 || strcmp(idMsg, RES_INF) == 0 || strcmp(idMsg, ERROR) == 0 || strcmp(idMsg, OK) == 0) {
		sprintf(message, "%s %s", message, payload);
	}
	sprintf(message, "%s\n", message);
	send(destinationType == DESTINATION_EQ_ID ?  threadSocketsMap[destinationId] : destinationId, message, strlen(message), 0);
}


void _sendEqList(int equipId) {
	bool first = true;
	char payload[MAX_BYTES] = { 0 };
	for(int i = 0; i < MAX_EQUIPMENTS; i++) {
		if(equipments[i]) {
			sprintf(payload, "%s%s%d", payload, first ? "" : ",",  i);
			first = false;
		}
	}
	_sendMessage(RES_LIST, -1, equipId, payload, DESTINATION_EQ_ID);
}

// Broadcast the added equipment to all the clients
bool _handleAddEquipment(int equipId) {
	for(int i = 0; i < MAX_EQUIPMENTS; i++) {
		if(!busyThreads[i]) continue;
		char* addedEquipId = malloc(sizeof(char) * 2);
		sprintf(addedEquipId, "%s%d", equipId < 10 ? "0" : "", equipId);
		_sendMessage(RES_ADD, -1, i, addedEquipId, DESTINATION_EQ_ID);
		free(addedEquipId);
	}
	equipments[equipId] = true;
	_sendEqList(equipId);
	return true;
}


// Parse the message and delegate the action to the correct function
bool _handleMessage(int equipId, char *message) {
	char **tokens = malloc(sizeof(char *) * MAX_TOKENS);
	int tc; split(message, tokens, &tc, " ");

	if(tc < 1) {
		return -1;
	}

	char* command = tokens[0];
	// if command is an equipment registering
	if(strcmp(command, REQ_ADD) == 0) { 
		return _handleAddEquipment(equipId);
	}

	return -1;
}


void *threadConnection(void *arg) {
	threadArgs tArgs = *((threadArgs *) arg);
	busyThreads[tArgs.threadId] = true;
	threadSocketsMap[tArgs.threadId] = tArgs.sockId;

	while(true) {
		// Receive and print message from client
		char buffer[MAX_BYTES] = { 0 };
		int valread = read(tArgs.sockId, buffer, MAX_BYTES);

		if(valread == 0) {
			printf("disconnecting...\n");
			busyThreads[tArgs.threadId] = false;
			equipments[tArgs.threadId] = false;
			break;
			// TODO update all the equipments with this disconnection
		}
		
		printf("(debug) buffer: %s\n", buffer);

		if(strcmp(buffer, "kill\n") == 0) {
			close(tArgs.sockId);
			break;
		}

		if(!_handleMessage(tArgs.threadId, buffer)) {
			// If the message is invalid, disconnect the client and wait for a new connection
			close(tArgs.sockId);
			printf("disconnecting...\n");
			busyThreads[tArgs.threadId] = false;
			break;
		}
	}
	int* returnMessage;
	return returnMessage;
}


int main(int argc, char const* argv[]) {
	Parameters *p = malloc(sizeof(Parameters));
	if(!initProgram(p, false, argc, argv)) {
		return 1;
	}
	_initEquipments();
	int protocol = AF_INET;

	int server_fd, new_socket;

	int opt = 1;
	size_t addrlen;

	struct sockaddr_storage addDestStorage;
  	memset(&addDestStorage, 0, sizeof(addDestStorage));
	struct sockaddr_in *address4 = (struct sockaddr_in *) &addDestStorage;
	address4->sin_family = AF_INET;
	address4->sin_addr.s_addr = INADDR_ANY;
	address4->sin_port = htons(p->port);
	addrlen = sizeof(*address4);

	// Initialize the client Address with the correct configurations for the selected protocol
 	struct sockaddr *destAddress = (struct sockaddr *) &addDestStorage;
	
	// Creates socket file descriptor
	if ((server_fd = socket(protocol, SOCK_STREAM, IPPROTO_TCP)) == 0) {
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	// Helps manipulating options for the socket
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}
	

	// Attaches socket to address and port
	if (bind(server_fd, destAddress, addrlen) < 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	if (listen(server_fd, 3) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

	// Wait for socket connections from the client
	while(true){
		if ((new_socket = accept(server_fd, destAddress, (socklen_t*) &addrlen)) < 0) {
			perror("accept");
			exit(EXIT_FAILURE);
		}
		// Create a new thread of the client
		int newThreadId = threadId();
		if(newThreadId == -1) {
			_sendMessage(ERROR, -1, new_socket, ERR_EQUIPMENT_LIMIT_EXCEEDED, DESTINATION_SOCKET_ID);
			close(new_socket);
			continue;
		}
		threadArgs tArgs;
		tArgs.sockId = new_socket;
		tArgs.threadId = newThreadId;
		pthread_create(&(threads[newThreadId]), NULL, threadConnection, &tArgs);
	}
	

	
	return 0;
}

