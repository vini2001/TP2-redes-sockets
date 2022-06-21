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

#define DESTINATION_EQ_ID -1

struct threadArgs {
	int sockId;
	int threadId;
};
typedef struct threadArgs threadArgs;

// Struct to hold the data of an equipment

bool equipments[MAX_EQUIPMENTS + 1];

pthread_t threads[MAX_EQUIPMENTS + 1];
bool busyThreads[MAX_EQUIPMENTS + 1];
int threadSocketsMap[MAX_EQUIPMENTS + 1];

int threadId() {
	int i;
	for(i = 1; i < MAX_EQUIPMENTS + 1; i++) {
		if(!busyThreads[i]) {
			return i;
		}
	}
	return -1;
}

// Init the equipments without any sensors
void _initEquipments() {
	for(int i = 1; i < MAX_EQUIPMENTS + 1; i++) {
		equipments[i] = false;
		busyThreads[i] = false;
	}
}


void _sendMessage(char* idMsg, int originEqId, int destinationEqId, char* payload, int destinationId) {
	char message[MAX_BYTES] = { 0 };
	sprintf(message, "%s", idMsg);

	if(strcmp(idMsg, REQ_REM) == 0 || strcmp(idMsg, REQ_INF) == 0 || strcmp(idMsg, RES_INF) == 0) {
		sprintf(message, "%s %s%d", message, originEqId < 10 ? "0" : "", originEqId);
	}

	if(strcmp(idMsg, REQ_INF) == 0 || strcmp(idMsg, RES_INF) == 0 || (strcmp(idMsg, ERROR) == 0 && strcmp(payload, ERR_EQUIPMENT_LIMIT_EXCEEDED) != 0) || strcmp(idMsg, OK) == 0) {
		sprintf(message, "%s %s%d", message, destinationEqId < 10 ? "0" : "", destinationEqId);
	}

	if(strcmp(idMsg, RES_ADD) == 0 || strcmp(idMsg, RES_LIST) == 0 || strcmp(idMsg, RES_INF) == 0 || strcmp(idMsg, ERROR) == 0 || strcmp(idMsg, OK) == 0) {
		sprintf(message, "%s %s", message, payload);
	}
	sprintf(message, "%s\n", message);
	send(destinationId == DESTINATION_EQ_ID ?  threadSocketsMap[destinationEqId] : destinationId, message, strlen(message), 0);
}


void _sendEqList(int equipId) {
	bool first = true;
	char payload[MAX_BYTES] = { 0 };
	for(int i = 1; i < MAX_EQUIPMENTS + 1; i++) {
		if(equipments[i]) {
			sprintf(payload, "%s%s%d", payload, first ? "" : ",",  i);
			first = false;
		}
	}
	_sendMessage(RES_LIST, -1, equipId, payload, DESTINATION_EQ_ID);
}

// Broadcast the added equipment to all the clients
bool _handleAddEquipment(int equipId) {
	for(int i = 1; i < MAX_EQUIPMENTS + 1; i++) {
		if(!busyThreads[i]) continue;
		char* addedEquipId = malloc(sizeof(char) * 2);
		sprintf(addedEquipId, "%s%d", equipId < 10 ? "0" : "", equipId);
		_sendMessage(RES_ADD, -1, i, addedEquipId, DESTINATION_EQ_ID);
		free(addedEquipId);
	}
	printf("Equipment %s%d added\n", equipId < 10 ? "0" : "", equipId);
	equipments[equipId] = true;
	_sendEqList(equipId);
	return true;
}

void _broadcastEquipmentRemoved(int toRemove) {
	for(int i = 1; i < MAX_EQUIPMENTS + 1; i++) {
		if(busyThreads[i]) {
			_sendMessage(REQ_REM, toRemove, -1, "", threadSocketsMap[i]);
		}
	}
}

bool _handleRemoveEquipment(int toRemove, int originEqId) {
	if(!equipments[toRemove]) {
		_sendMessage(ERROR, -1, -1, ERR_EQUIPMENT_NOT_FOUND, threadSocketsMap[originEqId]);
	}else{
		equipments[toRemove] = false;
		busyThreads[toRemove] = false;
		_sendMessage(OK, -1, originEqId, SUCCESSFUL_REMOVAL, threadSocketsMap[originEqId]);
		close(threadSocketsMap[originEqId]);
		printf("Equipment %s%d removed\n", toRemove < 10 ? "0" : "", toRemove);

		_broadcastEquipmentRemoved(toRemove);
	}

	return true;
}

bool _handleEquipmentInfo(int originEqId, int destinationEqId, int realEqId) {
	if(originEqId > MAX_EQUIPMENTS || !equipments[originEqId]) {
		_sendMessage(ERROR, originEqId, destinationEqId, ERR_SOURCE_EQUIPMENT_NOT_FOUND, threadSocketsMap[realEqId]);
		printf("Equipment %s%d not found\n", originEqId < 10 ? "0" : "", originEqId);
		return false;
	}

	if(destinationEqId > MAX_EQUIPMENTS || !equipments[destinationEqId]) {
		_sendMessage(ERROR, originEqId, destinationEqId, ERR_TARGET_EQUIPMENT_NOT_FOUND, threadSocketsMap[realEqId]);
		printf("Equipment %s%d not found\n", destinationEqId < 10 ? "0" : "", destinationEqId);
		return false;
	}

	_sendMessage(REQ_INF, originEqId, destinationEqId, "", DESTINATION_EQ_ID);
	return true;
}

bool _handleResEquipmentInfo(int originEqId, int destinationEqId, char* payload, int realEqId) {
	if(originEqId > MAX_EQUIPMENTS || !equipments[originEqId]) {
		_sendMessage(ERROR, originEqId, destinationEqId, ERR_SOURCE_EQUIPMENT_NOT_FOUND, threadSocketsMap[realEqId]);
		printf("Equipment %d not found\n", originEqId);
		return false;
	}

	if(destinationEqId > MAX_EQUIPMENTS || !equipments[destinationEqId]) {
		_sendMessage(ERROR, originEqId, destinationEqId, ERR_TARGET_EQUIPMENT_NOT_FOUND, threadSocketsMap[realEqId]);
		printf("Equipment %d not found\n", destinationEqId);
		return false;
	}

	_sendMessage(RES_INF, originEqId, destinationEqId, payload, threadSocketsMap[originEqId]);
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

	int subtSize = tc - 1;
	char **subtokens = malloc(sizeof(char *) * subtSize);
	for(int i = 1; i < subtSize + 1; i++) {
		subtokens[i - 1] = tokens[i];
	}

	// if command is an equipment registering
	if(strcmp(command, REQ_ADD) == 0) { 
		return _handleAddEquipment(equipId);
	} else if(strcmp(command, REQ_REM) == 0) { 
		return _handleRemoveEquipment(atoi(subtokens[0]), equipId);
	} else if(strcmp(command, REQ_INF) == 0) { 
		return _handleEquipmentInfo(atoi(subtokens[0]), atoi(subtokens[1]), equipId);
	} else if(strcmp(command, RES_INF) == 0) { 
		return _handleResEquipmentInfo(atoi(subtokens[0]), atoi(subtokens[1]), subtokens[2], equipId);
	}

	free(subtokens);
	return -1;
}


void *threadConnection(void *arg) {
	threadArgs tArgs = *((threadArgs *) arg);
	busyThreads[tArgs.threadId] = true;
	threadSocketsMap[tArgs.threadId] = tArgs.sockId;

	while(busyThreads[tArgs.threadId]) {
		// Receive and print message from client
		char buffer[MAX_BYTES] = { 0 };
		int valread = read(tArgs.sockId, buffer, MAX_BYTES);

		if(valread == 0) {
			printf("Equipment %s%d removed\n", tArgs.threadId < 10 ? "0" : "", tArgs.threadId);
			busyThreads[tArgs.threadId] = false;
			equipments[tArgs.threadId] = false;
			close(tArgs.sockId);
			_broadcastEquipmentRemoved(tArgs.threadId);
			break;
		}
		
		if(debug) {
			printf("(debug) buffer: %s\n", buffer);
		}

		_handleMessage(tArgs.threadId, buffer);
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
			_sendMessage(ERROR, -1, -1, ERR_EQUIPMENT_LIMIT_EXCEEDED, new_socket);
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

