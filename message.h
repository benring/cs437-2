/******************************************************************************
 *  File:   message.h
 *
 *  Author: Benjamin Ring & Josh Wheeler
 *  Date:   17 September 2014
 *  Description:  Define all message structures and associated functions for 
 *      preparing (encapsulating) and receiving (de-wrapping) message packets
 *      Also defines all common variable and values used for both send & receive
 ********************************************************************************/


#ifndef MESSAGE_H
#define MESSAGE_H

#include "config.h"

#define MAX_PAYLOAD_SIZE (MAX_PACKET_SIZE-(sizeof(char)+sizeof(int)))

/*  MESSAGE TAGS  */
#define DATA_MSG 'D'
#define STATUS_MSG 'S'
#define NAK_MSG 'N' 
#define EOM_MSG 'E'
#define GO_MSG 'G'
#define KILL_MSG 'K'

/*  Basic Message Struct:  TAG, SOURCE, & PAYLOAD  */
typedef struct Message {
	char tag;
	int pid;
	int payload[MAX_PAYLOAD_SIZE];
} Message;


typedef struct Value {
	char active;
	int lts;
	int data;
} Value;

/*
 * DATA MESSAGE:
 * 	payload[0] = message ID (mid)
 *  payload[1] = Lamport TimeStamp (LTS)
 *  payload[2] = Data Value
 * 
 * STATUS MESSAGE:
 *  payload[0:MAX_MACHINES-1] = mid
 *  payload[MAX_MACHINES] = Message ID of last packet (if already sent); otherwise -1
 * 
 * NAK MESSAGE:
 *  payload[0] = # lost msgs for PID 0
 *  payload[1..9] = lost messages, PID 0
 *  payload[10] = pid1
 *  payload[20] = pid2
 *  etc.....
*/









#endif