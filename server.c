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

#define MAX_EQUIPMENTS 5

struct threadArgs {
	int sockId;
	int threadId;
};
typedef struct threadArgs threadArgs;

// Struct to hold the data of an equipment
struct equipment {
	bool sensors[4];
	int* sensorList;
	int sensorCount;
};
typedef struct equipment equipment;

equipment equipments[MAX_EQUIPMENTS];
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
		memset(equipments[i].sensors, false, sizeof(equipments[i].sensors));
		equipments[i].sensorList = malloc(sizeof(int) * 4);
		equipments[i].sensorCount = 0;
		busyThreads[i] = false;
	}
}

// Mark the sensor as added and add it to the sensorList so we preserve the order they were added
void _addSensor(int equipmentId, int sensor) {
	equipmentId--;
	equipment *eq = &equipments[equipmentId];
	eq->sensors[sensor-1] = true;
	eq->sensorList[eq->sensorCount++] = sensor;
	sensorCount++;
}

// Mark the sensor as not added and remove it from the list. If it was not the last sensor
// in the list, we need to shift the right part of the list one index to the left
void _removeSensor(int eqId, int sensor) {
	equipment* eq = &equipments[eqId-1];
	eq->sensors[sensor-1] = false;
	for(int i = 0; i < eq->sensorCount; i++) {
		if(eq->sensorList[i] == sensor) {
			// remove gaps int the list
			for(int j = i; j < 3; j++) {
				eq->sensorList[j] = eq->sensorList[j+1];
			}
			eq->sensorCount--;
			sensorCount--;
			break;
		}
	}
}


// Validate if the sensors passed in a command are all valid
bool validSensors(char **tokens, int count) {
	for(int i = 0; i < count; i++) {
		int sensor = atoi(tokens[i]);
		if(sensor < SENSOR_RANGE_FROM || sensor > SENSOR_RANGE_TO) {
			return false;
		}
	}
	return true;
}

// Broadcast the added equipment to all the clients
char* _handleAddEquipment(int equipId) {
	char* response = malloc(sizeof(char) * 100);
	sprintf(response, "Equipment %d added\n", equipId);
	for(int i = 0; i < MAX_EQUIPMENTS; i++) {
		if(!busyThreads[i]) continue;
		send(threadSocketsMap[i], response, strlen(response), 0);
	}
	return "";
}

// Do the required verifications and add a sensor to an equipment
char* _handleAddSensor(char** tokens, int count, char* equipmentIdStr) {
	int equipmentId = atoi(equipmentIdStr);
	if(equipmentId < EQUIPMENT_RANGE_FROM || equipmentId > EQUIPMENT_RANGE_TO) {
		return "invalid equipment\n";
	}
	equipment *eq = &equipments[equipmentId - 1];

	if(!validSensors(tokens, count)) return "invalid sensor\n";

	char* successMessage = malloc(sizeof(char) * 100);
	char* errorMessage = malloc(sizeof(char) * 100);
	char* returnMessage = malloc(sizeof(char) * 100);
	sprintf(returnMessage, "%s ", "sensor");

	int errorQtt = 0;
	for(int i = 0; i < count; i++) {
		int sensor = atoi(tokens[i]);
		if(eq->sensors[sensor - 1]) {
			errorQtt++;
			sprintf(errorMessage, "%s%s ", errorMessage, tokens[i]);
		}
	}


	if(sensorCount + count - errorQtt > 15) {
		return "limit exceeded\n";
	}
	

	bool addAny = false;
	for(int i = 0; i < count; i++) {
		int sensor = atoi(tokens[i]);
		if(eq->sensors[sensor - 1]) continue;
		_addSensor(equipmentId, sensor);
		sprintf(successMessage, "%s%s ", successMessage, tokens[i]);
		addAny = true;
	}

	if(addAny) {
		sprintf(returnMessage, "%s%sadded%s", returnMessage, successMessage, errorQtt > 0 ? " " : "");
	}
	if(errorQtt > 0) {
		sprintf(returnMessage, "%s%salready exists in %s", returnMessage, errorMessage, equipmentIdStr);
	}
	sprintf(returnMessage, "%s\n", returnMessage);

	
	return returnMessage;
}

	


// Do the required verifications and read data from the sensors
// Note that the data read is random
char* _handleReadSensors(char** tokens, int count, char* equipmentIdStr) {
	int equipmentId = atoi(equipmentIdStr);
	if(equipmentId < EQUIPMENT_RANGE_FROM || equipmentId > EQUIPMENT_RANGE_TO) {
		return "invalid equipment\n";
	}
	equipment *eq = &equipments[equipmentId - 1];
	if(!validSensors(tokens, count)) return "invalid sensor\n";

	char* successMessage = malloc(sizeof(char) * 100);
	char* errorMessage = malloc(sizeof(char) * 100);
	bool errorAny = false;
	for(int i = 0; i < count; i++) {
		int sensor = atoi(tokens[i]);
		if(!eq->sensors[sensor - 1]) {
			errorAny = true;
			sprintf(errorMessage, "%s%s ", errorMessage, tokens[i]);
		}else{
			rand(); // discard first random number
			double value = ((double)rand() / (double)RAND_MAX)*10.0;
			sprintf(successMessage, "%s%.2f ", successMessage, value);
		}
	}

	char *returnMessage = malloc(sizeof(char) * 100);

	if(errorAny) {
		sprintf(returnMessage, "%ssensor(s) %snot installed\n", returnMessage, errorMessage);
	}else {
		sprintf(returnMessage, "%s%s\n", returnMessage, successMessage);
	}

	return returnMessage;
}


// Do the required verifications and remove a sensor from an equipment
char* _handleRemoveSensor(char** tokens, int count, char* equipmentIdStr) {
	int equipmentId = atoi(equipmentIdStr);
	if(equipmentId < EQUIPMENT_RANGE_FROM || equipmentId > EQUIPMENT_RANGE_TO) {
		return "invalid equipment\n";
	}
	equipment *eq = &equipments[equipmentId - 1];

	char *returnMessage = malloc(sizeof(char) * 100);

	if(!validSensors(tokens, count)) return "invalid sensor\n";
	if(count > 1) return "invalid";

	int sensor = atoi(tokens[0]);
	if(eq->sensors[sensor - 1]) {
		_removeSensor(equipmentId, sensor);
		sprintf(returnMessage, "sensor %s removed\n", tokens[0]);
	}else{
		sprintf(returnMessage, "sensor %s does not exist in %s\n", tokens[0], equipmentIdStr);
	}

	return returnMessage;
}

// Do the required verifications and list all the sensors attached to an equipment
char* _handleListSensors(int equipmentId) {
	if(equipmentId < EQUIPMENT_RANGE_FROM || equipmentId > EQUIPMENT_RANGE_TO) {
		return "invalid equipment\n";
	}
	equipment *eq = &equipments[equipmentId - 1];
	char *returnMessage = malloc(sizeof(char) * 100);
	if(eq->sensorCount == 0) return "none\n";

	for(int i = 0; i < eq->sensorCount; i++) {
		sprintf(returnMessage, "%s0%d ", returnMessage, eq->sensorList[i]);
	}
	sprintf(returnMessage, "%s\n", returnMessage);
	return returnMessage;
}

// Parse the message and delegate the action to the correct function
char* _handleMessage(int equipId, char *message) {
	char **tokens = malloc(sizeof(char *) * MAX_TOKENS);
	int tc; split(message, tokens, &tc);

	if(tc < 1) {
		return "invalid";
	}

	char* command = tokens[0];
	// if command is an equipment registering
	if(strcmp(command, REQ_ADD) == 0) { 
		return _handleAddEquipment(equipId);
	}


	// if commmand is "add"
	// if(strcmp(tokens[1], "sensor") == 0  && strcmp(tokens[tc-2], "in") == 0) {
	// 	int subtSize = tc - 4;
	// 	char **subtokens = malloc(sizeof(char *) * subtSize);
	// 	for(int i = 2; i < tc - 2; i++) {
	// 		subtokens[i - 2] = tokens[i];
	// 	}

	// 	if(strcmp(command, "add") == 0) {
	// 		return _handleAddSensor(subtokens, subtSize, tokens[tc-1]);
	// 	}else if(strcmp(command, "remove") == 0) {
	// 		return _handleRemoveSensor(subtokens, subtSize, tokens[tc-1]);
	// 	}
	// }else if(strcmp(tokens[0], "list") == 0 && strcmp(tokens[1], "sensors") == 0  && strcmp(tokens[tc-2], "in") == 0 && tc == 4) {
	// 	return _handleListSensors(atoi(tokens[tc-1]));
	// }else if(strcmp(tokens[0], "read") == 0 && strcmp(tokens[tc-2], "in") == 0) {
	// 	int subtSize = tc - 3;
	// 	char **subtokens = malloc(sizeof(char *) * subtSize);
	// 	for(int i = 1; i < tc - 2; i++) {
	// 		subtokens[i - 1] = tokens[i];
	// 	}
	// 	return _handleReadSensors(subtokens, subtSize, tokens[tc-1]);
	// }

	return "invalid";
}


void *threadConnection(void *arg) {
	threadArgs tArgs = *((threadArgs *) arg);
	busyThreads[tArgs.threadId] = true;
	threadSocketsMap[tArgs.threadId] = tArgs.sockId;

	while(true) {
		// Receive and print message from client
		char buffer[MAX_BYTES] = { 0 };
		int valread = read(tArgs.sockId, buffer, MAX_BYTES);
		printf("%s", buffer);

		if(strcmp(buffer, "kill\n") == 0) {
			close(tArgs.sockId);
			break;
		}

		char* message = _handleMessage(tArgs.threadId, buffer);
		if(strcmp(message, "invalid") == 0) {
			// If the message is invalid, disconnect the client and wait for a new connection
			close(tArgs.sockId);
			printf("disconnecting...\n");
			busyThreads[tArgs.threadId] = false;
			break;
		}else if(strcmp(message, "") != 0) {
			// Send the message returned by the function to the client
			send(tArgs.sockId, message, strlen(message), 0);
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
			//TODO: limit exceeded
			send(new_socket, "limit exceeded TODO: change this\n", strlen("limit exceeded TODO: change this\n"), 0);
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

