#pragma once

#ifndef RELAYCHAT_H
#define RELAYCHAT_H


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>


//OPCODES
#define IRC_OPCODE_ERR				0x10000001
#define IRC_OPCODE_HEARTBEAT		0x10000002
#define IRC_OPCODE_HELLO			0x10000003
#define IRC_OPCODE_LIST_ROOMS		0x10000004
#define IRC_OPCODE_LIST_ROOMS_RESP	0x10000005	
#define IRC_OPCODE_LIST_USERS_RESP	0x10000006
#define IRC_OPCODE_JOIN_ROOM		0x10000007
#define IRC_OPCODE_LEAVE_ROOM		0x10000008
#define IRC_OPCODE_SEND_MSG			0x10000009
#define IRC_OPCODE_TELL_MSG			0x1000000A
#define IRC_OPCODE_SEND_PRIV_MSG	0x1000000B
#define IRC_OPCODE_TELL_PRIV_MSG	0x1000000C
#define IRC_OPCODE_KEY_EXCHANGE		0x1000000D


//ERROR CODES
#define IRC_ERR_UNKNOWN			0x20000001
#define IRC_ERR_ILLEGAL_OPCODE	0x20000002
#define IRC_ERR_ILLEGAL_LENGTH	0x20000003
#define IRC_ERR_NAME_EXISTS		0x20000004
#define IRC_ERR_ILLEGAL_NAME	0x20000005
#define IRC_ERR_ILLEGAL_MESSAGE	0x20000006
#define IRC_ERR_TOO_MANY_USERS	0x20000007
#define IRC_ERR_TOO_MANY_ROOMS	0x20000008
#define IRC_ERR_WRONG_VERSION	0x20000009
#define IRC_ERR_NO_HEARTBEAT	0x2000000A

#define MAXMSGLENGTH 8000

//Generic Messages
struct irc_packet_header {
	uint32_t opcode;
	uint32_t length;
};

//do we actually need this data type? it seems like it's just a pattern
//for the other packets defined below
struct irc_packet_generic {
	struct irc_packet_header header;
	uint8_t content[];//header.length];
};

// Heartbeat Messages
struct irc_packet_heartbeat {
	struct irc_packet_header header;// = 
	//	{.opcode = IRC_OPCODE_HEARTBEAT, .length = 0};
};

//Error Messages
struct irc_packet_error {
	struct irc_packet_header header; //=
		//{.opcode = IRC_OPCODE_ERR, .length = 4};
	uint32_t error_code;
};


// CLIENT MESSAGES

//Hello
struct irc_packet_hello {
	struct irc_packet_header header;// =
		//{.opcode = IRC_OPCODE_HELLO, .length = 24};
	uint32_t version;
	char username[20];
};

//Room Messages
struct irc_packet_list_rooms {
	struct irc_packet_header header;// = 
		//{.opcode = IRC_OPCODE_LIST_ROOMS, .length = 0};
};

struct irc_packet_join {
	struct irc_packet_header header;// = 
		//{.opcode = IRC_OPCODE_JOIN_ROOM, .length = 20};
	char room_name[20];
};

struct irc_packet_leave {
	struct irc_packet_header header;// = 
		//{.opcode = IRC_OPCODE_LEAVE_ROOM, .length = 20};
	char room_name[20];
};

// User Messages
struct irc_packet_send_msg {
	struct irc_packet_header header;// = 
		//{.opcode = IRC_OPCODE_SEND_MSG, .length = LENGTH};
	char target_name[20];
	char msg[];//LENGTH - 20];
};

// SERVER MESSAGES

// Room List Response
struct irc_packet_list_resp {
	struct irc_packet_header header;// = 
		//{.opcode = IRC_OPCODE_LIST_ROOMS_RESP, .length = LENGTH};
	char identifier[20];
	char item_names[];//(LENGTH/20) - 1][20];
};

// User Message Forwarding
struct irc_packet_tell_msg {
	struct irc_packet_header header;// =
		//{.opcode = IRC_OPCODE_TELL_MSG, .length = LENGTH};
	char target_name[20];
	char sending_user[20];
	char msg[];//LENGTH - 40];
};

int validate_string(char * text, int length);



#endif
