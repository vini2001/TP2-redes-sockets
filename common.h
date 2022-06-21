#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>

#define debug false

// Control Messages
#define REQ_ADD "01"
#define REQ_REM "02"
#define RES_ADD "03"
#define RES_LIST "04"

// Data Messages
#define REQ_INF "05"
#define RES_INF "06"

// Error or Confirmation Messages
#define ERROR "07"
#define OK "08"

// Error Codes
#define ERR_EQUIPMENT_NOT_FOUND "01"
#define ERR_SOURCE_EQUIPMENT_NOT_FOUND "02"
#define ERR_TARGET_EQUIPMENT_NOT_FOUND "03"
#define ERR_EQUIPMENT_LIMIT_EXCEEDED "04"

// Confirmation codes
#define SUCCESSFUL_REMOVAL "01"

#define MAX_EQUIPMENTS 5 // TODO mudar pra 15



typedef struct parameters Parameters;
struct parameters {
  int port;
  const char* ip;
};

#define MAX_BYTES 500


// Parse the intial arguments and initialize the program
bool initProgram(Parameters *p, bool client, int argc, const char *argv[]) {
	if(argc < 3 - (client ? 0 : 1)) {
		if(client){
			printf("Usage: %s <IP> <port>\n", argv[0]);
		}else{
			printf("Usage: %s <port>\n", argv[0]);
		}
		return false;
	}

	if(client) {
		p->ip = argv[1];
	}

	int port;
	sscanf(argv[client ? 2 : 1], "%d", &port);

	(*p).port = port;
	return true;
}

// Split a message into a list of strings
void split(char *message, char **tokens, int *count, char* splitChar) {
	char *token = strtok(message, splitChar);

	
	int c = 0;
   	while(token != NULL ) {
		tokens[c++] = token;
      	token = strtok(NULL, splitChar);
   	}
	// remove \n from last token
	tokens[c-1] = strtok(tokens[c-1], "\n");
	*count = c;
}