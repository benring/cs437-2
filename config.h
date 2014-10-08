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


/*  COMMON DECLARATIONS   */
#define TRUE 1
#define FALSE 0
#define ACTIVE 'A'
#define INACTIVE 'X'
#define TIMEOUT_IDLE 30
#define TIMEOUT_RECV 100000

#define MAX_MACHINES 10
#define STATUS_TRIGGER 4
#define NAK_TRIGGER 32

#define MAX_PACKET_SIZE 1212    /*  ?????? */
#define MAX_BUFFER_SIZE 8
#define BATCH_SIZE 4

/*  STATES  */
#define IDLE 0
#define SEND 1
#define RECV 2

#endif

