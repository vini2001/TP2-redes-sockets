#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include "common.h"

/// The number of position allocated to the split array of a message
#define MAX_TOKENS 20

#define CLOSE_CONNECTION_COMMAND "close connection"
#define LIST_EQUIPMENTS_COMMAND "list equipment"
#define REQUEST_INFO_COMMAND "request information from"

/// Store this equipment's id
int thisId = -1;

/// Store whether an id has been assigned to this equipment
bool idDefined = false;

/// Store which equipments are connected to the server
bool equipments[MAX_EQUIPMENTS + 1];

/// Id of the socket connection to communicate with the server
int sock = 0;

/**
 * Organize and send a message to the server.
 *
 * @param idMsg: The ID of the message type
 * @param originEqId: The origin equipment id, acordding to the message table on the specs
 * @param destinationEqId: The destination equipment id, acordding to the message table on the specs
 * @param payload: The payload of the message, acordding to the message table on the specs
 **/
void _sendMessage(char* idMsg, int originEqId, int destinationEqId, char* payload) {
	char message[MAX_BYTES] = { 0 };
	sprintf(message, "%s", idMsg);

	if(strcmp(idMsg, REQ_REM) == 0 || strcmp(idMsg, REQ_INF) == 0 || strcmp(idMsg, RES_INF) == 0) {
		sprintf(message, "%s %s%d", message, originEqId < 10 ? "0" : "", originEqId);
	}

	if(strcmp(idMsg, REQ_INF) == 0 || strcmp(idMsg, RES_INF) == 0) {
		sprintf(message, "%s %s%d", message, destinationEqId < 10 ? "0" : "", destinationEqId);
	}

	if(strcmp(idMsg, RES_INF) == 0) {
		sprintf(message, "%s %s", message, payload);
	}

	sprintf(message, "%s\n", message);
	send(sock, message, strlen(message), 0);
}

/**
 * Handle an error message
 * 
 * @param tokens : The array of tokens of the message
 * @param size : The size of the array of tokens
 */
void _handleError(char **tokens, int size) {
	char* errorType = tokens[size-1];
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

/**
 * Handle an OK message from the server
 * 
 * @param tokens : The array of tokens of the message
 * @param size : The size of the array of tokens
 */
void _handleOk(char **tokens, int size) {
	char* errorType = tokens[1];
	if(strcmp(errorType, SUCCESSFUL_REMOVAL) == 0) { 
		printf("Successful removal\n");
		exit(0);
	}
}

/**
 * Handle the message that is received when another equipment connects to the server
 * 
 * @param tokens : The array of tokens of the message (first position should be the added equipment's id)
 * @param size : The size of the array of tokens
 */
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

/**
 * Handle the message that is received when another equipment is removed or disconnected from the server
 * 
 * @param tokens : The array of tokens of the message (first position should be the removed equipment's id)
 * @param size : The size of the array of tokens
 */
void _handleEquipmentRemoved(char **tokens, int size) {
	int eqId = atoi(tokens[0]);
	
	printf("Equipment %s removed\n", tokens[0]);
	equipments[eqId] = false;
}

/**
 * Handle the message that is received when another equipment requests information from this equipment
 * 
 * @param tokens : The array of tokens of the message (first position should be the requester's id and the second position should be the requested equipment's id = this one)
 * @param size : The size of the array of tokens
 */
void _handleRequestInfo(char **tokens, int size) {
	int originId = atoi(tokens[0]);
	int destinationId = atoi(tokens[1]);
	
	printf("requested information\n");

	float value = ((float)rand() / (float)RAND_MAX)*10.0;
	char stringValue[5] = "00.00";
	sprintf(stringValue, "%.2f", value);
	_sendMessage(RES_INF, destinationId, originId, stringValue);
}


/**
 * Handle the message that is received when this equipment receives the requested information from another equipment
 * 
 * @param tokens : The array of tokens of the message (second position should be the id of the responding equipment and the third the value of the information)
 * @param size : The size of the array of tokens
 */
void _handleRequestResInfo(char **tokens, int size) {
	char* destinationId = tokens[1];
	char* payload = tokens[2];
	
	printf("Value from %s: %s\n", destinationId, payload);
}

/**
 * Handle the message that containing a list of all equipments connected to the server (fired as soon as this equipment is added)
 * 
 * @param tokens : The array of tokens of the message (first position should be a string containing the list of connected equipments separated by a comma(,))
 * @param size : The size of the array of tokens
 */
void _handleCurrentEquipmentList(char **tokens, int size) {
	char **ids = malloc(sizeof(char *) * MAX_TOKENS);
	int idCount; split(tokens[0], ids, &idCount, ",");
	
	for(int i = 0; i < idCount; i++) {
		int id = atoi(ids[i]);
		equipments[id] = true;
	}
}

/**
 * Handle the messages received from the server
 * 
 * @param message : A string representing the encoded message
 */
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
	} else if(strcmp(commandType, OK) == 0) {
		_handleOk(subtokens, subtSize);
	} else if(strcmp(commandType, REQ_REM) == 0) {
		_handleEquipmentRemoved(subtokens, subtSize);
	} else if(strcmp(commandType, REQ_INF) == 0) {
		_handleRequestInfo(subtokens, subtSize);
	} else if(strcmp(commandType, RES_INF) == 0) {
		_handleRequestResInfo(subtokens, subtSize);
	}
	free(tokens);
	free(subtokens);
}

/**
 * A thread that listens to the server and handles the messages received
 * 
 * @param arg {int*} : The id of the socket to listen to
 */
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

/**
 * List all the connected equipments
 */
void _listEquipments() {
	bool first = true;
	for(int i = 1; i < MAX_EQUIPMENTS + 1; i++) {
		if(equipments[i] && i != thisId) {
			if(!first) printf(" ");
			printf("%s%d", i < 10 ? "0" : "", i);
			first = false;
		}
	}
	printf("\n");
}

/**
 * Match the typed command with the corresponding function
 * 
 * @param command : The command to match
 * @param commandSize : The size of the command string
 */
void _executeCommand(char* command, size_t commandSize) {
	if(strstr(command, LIST_EQUIPMENTS_COMMAND) != NULL) {
		_listEquipments();
		return;
	}

	if(strstr(command, CLOSE_CONNECTION_COMMAND) != NULL) {
		_sendMessage(REQ_REM, thisId, -1, "");
		return;
	}

	if(strstr(command, REQUEST_INFO_COMMAND) != NULL) {
		char **parts = malloc(sizeof(char *) * 4);
		int partsCount; split(command, parts, &partsCount, " ");
		char* requestInfEqId = parts[partsCount-1];
		_sendMessage(REQ_INF, thisId, atoi(requestInfEqId), "");
		free(parts);
	} else {
		printf("Invalid command\n");
	}

	return;
}

int main(int argc, char const* argv[])
{

	// init rand seed
	srand(time(NULL));

	Parameters *p = malloc(sizeof(Parameters));
	if(!initProgram(p, true, argc, argv)) {
		return 1;
	}
	int protocol =AF_INET;

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

	_sendMessage(REQ_ADD, -1, -1, "");

	while(true) {
		size_t bufsize;
		char *command = malloc(bufsize * sizeof(char));
		getline(&command, &bufsize, stdin);
		_executeCommand(command, bufsize);
		free(command);
	}
	return 0;
}

