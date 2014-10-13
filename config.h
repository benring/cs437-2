#ifndef	CONFIG_H
#define CONFIG_H

/*#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h> 
#include <netdb.h>
#include <errno.h>
#include <dirent.h>
*/
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


/*  COMMENT OUT FOLLOWING LINE TO REMOVE DEBUGGING */
#define DEBUG 1 

#ifdef  DEBUG
#define printdb(args...) printf(args);
#else
#define printdb(args...)
#endif

#define NUM_PACKET_DEBUG 5000


/*  COMMON DECLARATIONS   */
#define TRUE 1
#define FALSE 0
#define UNKNOWN 0
#define INACTIVE 1
#define ACTIVE 2
#define DONE_DELIVERING 3
#define DONE_SENDING 4
#define COMPLETING 5

#define TIMEOUT_IDLE 30
#define TIMEOUT_RECV 50000

#define MAX_MACHINES 10
#define STATUS_TRIGGER 16
#define NAK_TRIGGER 64
#define NAK_QUOTA 20


#define MAX_PACKET_SIZE 1212   
#define MAX_BUFFER_SIZE 64
#define BATCH_SIZE 2

/*  STATES  */
#define IDLE 0
#define SEND 1
#define RECV 2
#define KILL 3

#endif

