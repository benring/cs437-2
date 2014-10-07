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

#define MAX_PAYLOAD_SIZE (MAX_PACKET_SIZE-(sizeof(char)+sizeof(int))

/*  MESSAGE TAGS  */
#define DATA_MSG 'D'
#define STATUS_MSG 'A'
#define NAK_MSG 'H'
#define EOM_MSG 'E'


/*  Basic Message Struct:  TAG, SOURCE, & PAYLOAD  */
typedef struct Message {
	char tag;
	int pid;
	int payload[MAX_PAYLOAD_SIZE];
} Message;


typedef struct Value {
	int lts;
	int data;
}

/*
 * DATA MESSAGE:
 * 	payload[0] = message ID (mid)
 *  payload[1] = Lamport TimeStamp (LTS)
 *  payload[2] = Data Value
 * 
 * STATUS MESSAGE:
 *  payload[0:MAX_MACHINES] = mid
 * 
 * NAK MESSAGE:
 *  payload[0] = # machines
 *  payload[1] = # lost message for pid0
 *  payload[2..n] = pid0 lost message ID's
 *  payload[n+1] = # lost messages for pid1
 *  etc.....
*/

/**  DO NOT THINK THESE ARE NECESSARY  ***/
typedef struct DataMessage {
	int mid;
	int data;
} DataMessage;


typedef struct StatusMessage {
	int mid[MAX_MACHINES];
} StatusMessage;


typedef struct NAKMessage {
	int * nak;
} NAKMessage;
*/

/******* =========>>>>  HERE  COMPLETE MESSAGE STRUCTS   <<<<<<======= ***/

/* prepareRequestToSend: Build a R2X in Message 'm' payload */
void prepareRequestToSend(Message * m, char * filename);

/* prepareWait: Build a Wait Packet in Message 'm' */
void prepareWait(Message * m);

/* prepareEOF: Build a EOF Packet in Message 'm' */
void prepareEOF(Message * m);

/* prepareTerm: Build TERM Packet in Message 'm' */
void prepareTerm(Message * m);

/* prepareAck: Build ACK Packet in Message 'm' with RN and WinMask*/
void prepareAck(Message * m, unsigned int rn, char * wm);

/*  prepareData: Build Data Packet with given SN, size, & data bits  */
void prepareData (Message * m, unsigned int sn, unsigned int len, char * data);

/*  processAck: retrieves Window Mask & RN
 * 		RETURN:  Stores window mask in wm & returns the ACK RN */
int processAck (Message * m, char *wm);

/*  checkAck: peeks at the ACK for the SN
 * 		RETURN:  ACK request number */
int checkAck (Message * m);

/*  Update the file name in place.  */
void processFileIDMessage (Message * m, char *file_id);

/*  processData:  retrieves data, SN, & size from the given packet */
void processData (Message * m, unsigned int *sn, int *len, char *data);








#endif