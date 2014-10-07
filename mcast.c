/****************************************************************************
 * File: mcast.c
 *
 * Author: Benjamin Ring & Josh Wheeler
 * Description:  The mutlicast process 
 *
 * *************************************************************************/


int gethostname(char*, size_t);

/****************************************
 *   MAIN PROGRAM EXECUTION  
 ****************************************/

int main(int argc, char*argv[])
{

    /*----------------------------------------
     * VAR DECLARATIONS
     *----------------------------------------*/

	/* Networking vars */
	struct sockaddr_in    name;
	struct sockaddr_in    send_addr;
	struct sockaddr_in    from_addr;
	socklen_t             from_len;
	struct hostent        h_ent;
	struct hostent        *p_h_ent;
	char * host_name;
	int                   host_num;
	int                   ss,sr;
	fd_set                mask;
	fd_set                dummy_mask,temp_mask;
	int                   bytes;
	int                 	sock_num;
	char                	mess_buf[MAX_PACKET_SIZE];
	struct timeval  		timeout;        

	/* Message structs for interpreting bytes */
	Message         *out_msg;
	Message			*in_msg;

	/* Process's Own State */
	int				me;
	int				num_machines;
	int				lts;
	int				max_order_lts;
	
	/* All Processes States  */
	int				mid[MAX_MACHINES];
	
	/*  File I/O vals  */
	FILE            *dest;
	char            *dest_file;
	unsigned int    file_size;
	char            *c;

	/*  Packet Handling  */
    size_t          data_size;
	char            data_buffer[MAX_PAYLOAD_SIZE];
	int             packet_loss;
	int				from_pid;

	/*  Other vars	*/
	int				index;



	/*------------------------------------------------------------
	*    (1)  Conduct program Initialization
	* --------------------------------------------------------------- */

	/*  Parse Command Line Args   
	if (argc != 5) {
		printf("usage: %s <num_of_packets> <machine_index> <num_of_machines> <loss_rate>\n", argv[0]);
		exit(-1);
	}
	  
	num_packets = atoi(argv[1]);
	me = atoi(argv[2]);
	num_machines = atoi(argv[3]);
	loss_rate = atoi(argv[4]);
	 */
	
	/*   recv_dbg_init(loss_rate, me);  */

	/* Open output file */
/*	printDB("\t Open output file");
	if ((source = fopen(dest_file, "w")) == NULL)  {
		perror("ERROR opening destination file");
		exit(0);
	}
*/
	/* Open & Bind socket for receiving */
/*	printDB("\t Open & Bind Receiving Socket");
	sr = socket(AF_INET, SOCK_DGRAM, 0);
	if (sr<0) {
		perror("Ucast: socket");
		exit(1);
	}
	name.sin_family = AF_INET;
	name.sin_addr.s_addr = INADDR_ANY;
	name.sin_port = htons(PORT);
	if ( bind( sr, (struct sockaddr *)&name, sizeof(name) ) < 0 ) {
		perror("Ucast: bind");
		exit(1);
	}
*/
	/* Open & Bind socket for sending */
	
/******  TODO:  SET UP SENDING SOCKET FOR MULTICAST **********/
	
/*	printDB("\t Open & Bind Sending Socket");
	ss = socket(AF_INET, SOCK_DGRAM, 0); 
	if (ss<0) {
		perror("Ucast: socket");
		exit(1);
	}
	p_h_ent = gethostbyname(host_name);
	if ( p_h_ent == NULL ) {
		printdb("Ucast: gethostbyname error.\n");
		exit(1);
	}
	memcpy( &h_ent, p_h_ent, sizeof(h_ent));
	memcpy( &host_num, h_ent.h_addr_list[0], sizeof(host_num) );

	send_addr.sin_family = AF_INET;
	send_addr.sin_addr.s_addr = host_num;
	send_addr.sin_port = htons(PORT);

	FD_ZERO( &mask );
	FD_ZERO( &dummy_mask );
	FD_SET( sr, &mask );
*/	

	state = IDLE;
	lts = 0;
	max_order_lts = 0;
	
	for (index=0; index < num_machines; index++) {
		mid[index] = 0;
	}

    /*------------------------------------------------------------------
     *
     *      MAIN ALGORITHM EXECUTION
     *
     *------------------------------------------------------------------*/

	for(;;) {
		/*---------------------------------------------------------
		 * (1)  Implement SEND Algorithm
		 *--------------------------------------------------------*/

		if (status_count >= STATUS_TRIGGER)  {
			status_count = 0;
			/*  SEND STATUS MESSAGE  */
		}
		
		if (nak_count >= NAK_TRIGGER)  {
			nak_count = 0;
			/* CHECK NAK HEURISTIC  */
			/*  SEND NAK MESSAGE  */
		}
		
		/*  Sending Algorithm:
		 * 		1. Create New Packets (if possible)
		 * 		2. Increment & Stamp LTS
		 * 		3. Store Message in buffer
		 * 		4. Send message
		  */
		while ((msg_count <= num_packets) && (!message_buffer[me].isFull)) {
			out_msg = create_message(++lts);
			message_buffer[me].append(out_msg());   /***  TODO:  WHAT DO WE ACTUALLY STORE????  ***/
			sendto(ss,(char *) &out_msg, sizeof(Message), 0,
			       (struct sockaddr *)&send_addr, sizeof(send_addr));
		}
		
		/*  Send Lost messages  */
		while (!lost_msg_list.isEmpty())  {
			out_msg = lost_msg_list.deq();
			sendto(ss,(char *) &out_msg, sizeof(Message), 0,
			       (struct sockaddr *)&send_addr, sizeof(send_addr));
		}
		
	
		/*---------------------------------------------------------
		 * (2)  Prepare & Receive Data from receiving socket
		 *--------------------------------------------------------*/
		/*  reset default timeout & mask vals  */
		temp_mask = mask;

		/*  IF IN IDLE STATE: set longer timeout  */
		if (state == IDLE)  {
			timeout.tv_sec = TIMEOUT_IDLE;
			timeout.tv_usec = 0;
		}
	
		/*  OTHERWISE: GOTO RECV state  */
		else  {
			state = RECV;	
			timeout.tv_sec = 0;
			timeout.tv_usec  = TIMEOUT_RECV;
		}

		/*  Receive Data  */
		sock_num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &timeout);
		if (num > 0 && FD_ISSET(sr, &temp_mask)) {
			from_len = sizeof(from_addr);

			/*** TODO:  change to recv_dbg()  ***/
			bytes = recvfrom(sr, mess_buf, sizeof(mess_buf), 0,
			                  (struct sockaddr *)&from_addr, &from_len );
			from_ip = from_addr.sin_addr.s_addr;
			in_msg = (Message *) mess_buf;
			printdb("received some data TAG=%c, FROM=%d\n", in_msg->tag, in_msg->src);
		} else {
			status_count = STATUS_TRIGGER;
			if (state == RECV)  {
				nak_count++;			/** TODO:  Determine NAK Handling  **/
				state == SEND;			
			}
			printdb("timed_out when trying to receive\n");
			continue;
		}

		from_pid = in_msg->pid;
		
		/*---------------------------------------------------------
		 * (3)  Implement RECV Algorithm
		 *--------------------------------------------------------*/
		switch (in_msg->tag)  {
		
			case DATA_MSG:
				/*  STORE DATA  */
				elm.lts = in_msg->payload[1];
				elm.data = in_msg->payload[2];
				/*  Update RECV State */
				recv_state[from_pid] = message_buffer[from_pid].insert(in_msg->payload[0], elm)
				break;
			
			
			case STATUS_MSG:
				/*  Update SEND State  */
				send_state[from_pid] = in_msg->payload[me];
				/*  CHECK LTS for message delivery  */

				break;
				
			
			case NAK_MSG:
				/*  Update LOST msg queue  */
				break;
				
			
			case EOM_MSG:
				/*  Process EOM Message  */
				break;
				
			default:
				printdb("Process Received erroneous message");
		}


	}
	return 0;
}