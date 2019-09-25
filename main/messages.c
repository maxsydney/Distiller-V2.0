#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <esp_err.h>
#include <string.h>
#include "messages.h"
#include "sensors.h"
#include "ds18b20.h"
#include "networking.h"
#include "controlLoop.h"
#include "ota.h"

#define MAX_LINE_LEN 40
#define MAX_KEY_LEN 10
#define MAX_VALUE_LEN 5

static char tag[] = "Messages";

Message* parseMessage(char* dataPacket, uint32_t len)
{
    Message* head = NULL;
    Message* curr = NULL;
    char* pair;
    
    pair = strtok(dataPacket, ",");

    while (pair != NULL) {
        if (head == NULL) {
            head = processPair(pair);
            curr = head;
        } else {
            curr->next = processPair(pair);
            curr = curr->next;
        }
        pair = strtok(NULL, ",");
    }

    return head;
}

Message* processPair(char* pair)
{   
    int i = 0;
    float value = 0;
    char valueBuf[MAX_VALUE_LEN];
    char* name = malloc(MAX_KEY_LEN * sizeof(char));
    Message* message = malloc(sizeof(Message));

    while (*pair != ':') {
        name[i++] = *pair;
        pair++;
    }
    name[i] = '\0';

    i = 0;
    pair++;

    while (*pair) {
        valueBuf[i++] = *pair;
        pair++;
    }
    valueBuf[i] = '\0';

	value = atof(valueBuf);

    message->key = name;
    message->value = value;
    message->next = NULL;

    return message;
}

void printMessages(Message* head)
{
	while (head) {
		printf("%s: %.2f\n", head->key, head->value);
		head = head->next;
	}
}

float findByKey(Message* head, char* key)
{
	while (head) {
		if (strncmp(key, head->key, MAX_KEY_LEN) == 0) {
			return head->value;
		}
		head = head->next;
	}
	return 0;
}

int freeMessages(Message* head)
{
	Message* curr;
	while (head) {
		curr = head;
		free(head->key);
		head = head->next;
		free(curr);
	}
	return 1;
}

Data* decode_data(char* dataPacket)
{
    
    uint16_t len = strlen(dataPacket) - 1; 
    Message* head = parseMessage(dataPacket, len);
    if (head == NULL) {
        printf("Could not read message\n");
        return 0;
    }

    Data* data = malloc(sizeof(Data));
    data->setpoint = findByKey(head, "setpoint");
    data->P_gain = findByKey(head, "P");
    data->I_gain = findByKey(head, "I");
    data->D_gain = findByKey(head, "D");
    freeMessages(head);
    return data;    
}

Cmd_t decodeCommand(char* commandPacket)
{
    char* command = strtok(commandPacket, ":");
    char* arg = strtok(NULL, "&\n");
    Cmd_t cmd;
    strncpy(cmd.cmd, command, CMD_LEN);
    strncpy(cmd.arg, arg, ARG_LEN);
    printf("Command: %s\nArg: %s\n", command, arg);

    return cmd;
}
