/****************************************************************************
 * File: mcast.c
 *
 * Author: Benjamin Ring & Josh Wheeler
 * Description:  The mutlicast process 
 *
 * *************************************************************************/
#include "config.h"
#include "message.h"

/****************************************
 *   MAIN PROGRAM EXECUTION  
 ****************************************/

int main(int argc, char*argv[])
{

    /*----------------------------------------
     * VAR DECLARATIONS
     *----------------------------------------*/

	/* Networking vars */
	struct sockaddr_in    send_addr;
	int                   mcast_addr;
	int					  port; 
	struct ip_mreq			mreq;
	unsigned char			ttl_val;
	int                   ss;
	int                 	sock_num;
	char                	mess_buf[MAX_PACKET_SIZE];
	struct timeval  		timeout;        
	
	/* Message structs for interpreting bytes */
	Message			start_msg;

	/*------------------------------------------------------------
	*    (1)  Conduct program Initialization
	* --------------------------------------------------------------- */

	mcast_addr = 225 << 24 | 1 << 16 | 2 << 8 | 114;
	port = 10140;


	/* Open & Bind socket for sending multicast */
	printdb("\t Open & Bind Sending Socket\n");
	ttl_val = 1;
	
	ss = socket(AF_INET, SOCK_DGRAM, 0); 
	if (ss<0) {
		perror("Mcast: socket");
		exit(1);
	}

	if (setsockopt(ss, IPPROTO_IP, IP_MULTICAST_TTL, 
		(void *)&ttl_val, sizeof(ttl_val)) < 0)  {
			perror("Mcast: problem with setting sending sock option");
		}
	send_addr.sin_family = AF_INET;
	send_addr.sin_addr.s_addr = htonl(mcast_addr);
	send_addr.sin_port = htons(port);

    /*------------------------------------------------------------------
     *    SEND ONE MESSAGE
     *------------------------------------------------------------------*/
	start_msg.tag = GO_MSG;
	start_msg.pid = -1;
	sendto(ss,(char *) &start_msg, sizeof(Message), 0,
			       (struct sockaddr *)&send_addr, sizeof(send_addr));
	return 0;
}