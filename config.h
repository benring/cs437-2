#ifndef	CONFIG_H
#define CONFIG_H


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>


/*  COMMENT OUT FOLLOWING LINE TO REMOVE DEBUGGING */
/*#define DEBUG 1  */
#ifdef  DEBUG
#define printdb(args...) printf(args);
#else
#define printdb(args...)
#endif

#define INFO 1 
#ifdef  INFO
#define printinfo(args...) printf(args);
#else
#define printinfo(args...)
#endif

#define ERR 1 
#ifdef  ERR
#define printerr(args...) printf(args);
#else
#define printerr(args...)
#endif

/*#define SEL_DEBUG 0 */
#ifdef  SEL_DEBUG
#define printsel(args...) if (SEL_DEBUG > 0) printf(args)
#else
#define printsel(args...)
#endif


#define NUM_PACKET_DEBUG 5000


/*  FIXED DECLARATIONS   */
#define TRUE 1
#define FALSE 0
#define MAX_MACHINES 10
#define MAX_PACKET_SIZE 1212   
#define MAX_PAYLOAD_SIZE (MAX_PACKET_SIZE-(sizeof(char)+sizeof(int)))
#define MCAST_ADDR (225 << 24 | 1 << 16 | 2 << 8 | 114)
#define PORT 10140

/* Discretionary Declarations */
#define MAX_BUFFER_SIZE 32
#define BATCH_SEND 2
#define BATCH_RECEIVE 1
#define STATUS_TRIGGER 128
#define NAK_TRIGGER 256
#define RESEND_TRIGGER 20
#define TRIGGER_ACCELERATOR 32
#define NAK_QUOTA 29
#define NAK_BACKOFF 16

/* Discretionary Declarations
#define MAX_BUFFER_SIZE 127
#define BATCH_SEND 1
#define BATCH_RECEIVE 8
#define STATUS_TRIGGER 64
#define NAK_TRIGGER 1024  */


/* Timeouts  */
#define TIMEOUT_IDLE 60   /* sec  */
#define TIMEOUT_RECV 50   /* usec */

/*  Process States  */
#define IDLE 0
#define SEND 1
#define RECV 2
#define PROCESS 3
#define KILL 4

/* Sending States */
#define INACTIVE 0
#define UNKNOWN 1
#define ACTIVE 2
#define DONE_DELIVERING 3
#define DONE_SENDING 4

/*  MESSAGE TAGS  */
#define DATA_MSG 'D'
#define STATUS_MSG 'S'
#define NAK_MSG 'N' 
#define EOM_MSG 'E'
#define GO_MSG 'G'
#define KILL_MSG 'K'

/* Other defs  */
/*#define DISPLAY_INTERVAL 1000 */

/*  Basic Message Struct:  TAG, SOURCE, & PAYLOAD  */
typedef struct Message {
	char tag;
	int pid;
	int payload[MAX_PAYLOAD_SIZE];
} Message;


typedef struct Value {
/*	int active; */
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
 *  payload[0:MAX_MACHINES-1] = Message ID of last delivered packet (ACK)
 *  payload[MAX_MACHINES] = Message ID of last packet (if already sent); otherwise -1
 *  payload[MAX_MACHINES+1:MAX_MACHINES*2] = status
 * 
 * NAK MESSAGE:
 *  payload[0] = # lost msgs for PID 0
 *  payload[1..9] = lost messages, PID 0
 *  payload[10] = pid1
 *  payload[20] = pid2
 *  etc.....
*/

#endif

