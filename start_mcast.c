/****************************************************************************
 * File: mcast.c
 * Author: Benjamin Ring & Josh Wheeler
 * Date: 15 October 2014
 * Description:  Start Signal Program to initiate the reliable Multicast 
 * 					protocol mutlicast process 
 *
 * *************************************************************************/
#include "config.h"

/****************************************
 *   MAIN PROGRAM EXECUTION  
 ****************************************/

int main(int argc, char*argv[])
{
	Message				start_msg;
	struct sockaddr_in  send_addr;
	unsigned char		ttl_val;
	int                 ss;
	

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
	send_addr.sin_addr.s_addr = htonl(MCAST_ADDR);
	send_addr.sin_port = htons(PORT);


    /*------------------------------------------------------------------
     *    SEND ONE MESSAGE
     *------------------------------------------------------------------*/
	start_msg.tag = GO_MSG;
	start_msg.pid = -1;
	sendto(ss,(char *) &start_msg, sizeof(Message), 0,
			       (struct sockaddr *)&send_addr, sizeof(send_addr));
	return 0;
}