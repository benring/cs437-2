/****************************************************************************
 * File: mcast.c
 *
 * Author: Benjamin Ring & Josh Wheeler
 * Description:  The mutlicast process 
 *
 * *************************************************************************/
#include "config.h"
#include "message.h"
#include "buffer.h"

/****************************************
 *   MAIN PROGRAM EXECUTION  
 ****************************************/

printarray(int *v, int n)  {
	int i;
	for (i=0; i<n; i++)  {
		printf("  %3d,", v[i]);
	}
	printf("\n");
}

int main(int argc, char*argv[])
{

    /*----------------------------------------
     * VAR DECLARATIONS
     *----------------------------------------*/

	/* Networking vars */
	struct sockaddr_in    recv_addr;
	struct sockaddr_in    send_addr;
	struct sockaddr_in    from_addr;
	socklen_t             from_len;
	int                   mcast_addr;
	int					  port; 
	struct ip_mreq			mreq;
	unsigned char			ttl_val;
	int                   ss,sr;
	fd_set                mask;
	fd_set                dummy_mask,temp_mask;
	int                   bytes;
	int                 	sock_num;
	char                	mess_buf[MAX_PACKET_SIZE];
	struct timeval  		timeout;        
	
	

	/* Message structs for interpreting bytes */
	Message         out_msg;
	Message			*in_msg;
	Message			test_msg;
	Value			*elm;

	/* Process's Internal State */
	int				me;
	int				num_machines;
	int				lts;
	int				max_order_lts;
	int				state;
	int				status_count;
	int				nak_count;
	int				max_batch;

	/*  Outgoing Message State (send)  */
	int				msg_count;
	int				num_packets;
	int				max_ack[MAX_MACHINES];

	
	/*  All Message State (receive)  */
	buffer			message_buffer[MAX_MACHINES];
	int				max_mid[MAX_MACHINES];
	int				max_lts[MAX_MACHINES];
	int				deliver_lts;
	int				have_delivered;
	
	
	/* All Processes States  */
	int				mid[MAX_MACHINES];
	
	/*  File I/O vals  */
	FILE            *sink;
	char            dest_file[10];
	unsigned int    file_size;
	char            *c;

	/*  Packet Handling  */
    size_t          data_size;
	char            data_buffer[MAX_PAYLOAD_SIZE];
	int             packet_loss;
	int				from_pid;

	/*  Other vars	*/
	int				index;
	int				min_lts;
	int	i;
	int				can_deliver;
	int				cur_minlts;
	int				cur_minpid;


	/*------------------------------------------------------------
	*    (1)  Conduct program Initialization
	* --------------------------------------------------------------- */

	mcast_addr = 225 << 24 | 1 << 16 | 2 << 8 | 114;
	port = 10140;

	if (argc != 2) {
		me = 0;
	}
	else {
		me = atoi(argv[1]);
	}
	num_packets = 20;
	sprintf(dest_file, "%d.out", me);
	num_machines = 3;
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
   


	printdb("\t Open output file");
	if ((sink = fopen(dest_file, "w")) == NULL)  {
		perror("ERROR opening destination file");
		exit(0);
	}
	/* Open & Bind socket for receiving */
	printdb("\t Open & Bind Receiving Socket\n");
	sr = socket(AF_INET, SOCK_DGRAM, 0);
	if (sr<0) {
		perror("Ucast: socket");
		exit(1);
	}
	recv_addr.sin_family = AF_INET;
	recv_addr.sin_addr.s_addr = INADDR_ANY;
	recv_addr.sin_port = htons(port);
	if ( bind( sr, (struct sockaddr *)&recv_addr, sizeof(recv_addr) ) < 0 ) {
		perror("Mcast: bind");
		exit(1);
	}

	mreq.imr_multiaddr.s_addr = htonl(mcast_addr);
	mreq.imr_interface.s_addr = INADDR_ANY;
	if (setsockopt(sr, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
		(void *)&mreq, sizeof(mreq))<0)  {
			perror("Mcast: problem with setting sock option");
		}
		
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

	FD_ZERO( &mask );
	FD_ZERO( &dummy_mask );
	FD_SET( sr, &mask );

	state = IDLE;
	lts = -1;
	deliver_lts = -1;
	have_delivered = -1;
	max_order_lts = 0;
	msg_count = 0;
	out_msg.pid = me;
	
	for (index=0; index < MAX_MACHINES; index++) {
		mid[index] = 0;
		max_mid[index] = -1;
		max_lts[index] = -1;
		max_ack[index] = -1;
	}

    /*------------------------------------------------------------------
     *
     *      MAIN ALGORITHM EXECUTION
     *
     *------------------------------------------------------------------*/

	for(i=0; i<200; i++) {  
		/*---------------------------------------------------------
		 * (1)  Implement SEND Algorithm
		 *--------------------------------------------------------*/

		if (state != IDLE)  {
			
			if (status_count >= STATUS_TRIGGER)  {
				
				status_count = 0;
				out_msg.tag = STATUS_MSG;
				for (index=0; index<MAX_MACHINES; index++) {
					out_msg.payload[index] = max_mid[index];
				}
				sendto(ss,(char *) &out_msg, sizeof(Message), 0,
					   (struct sockaddr *)&send_addr, sizeof(send_addr));
			
			}
			if (nak_count >= NAK_TRIGGER)  {
				nak_count = 0;
				out_msg.tag = NAK_MSG;
				/* CHECK NAK HEURISTIC  */
				/*  SEND NAK MESSAGE  */
			}
			
			/*  Sending Algorithm:
			 * 		1. Create New Packets (if possible)
			 * 		2. Increment & Stamp LTS
			 * 		3. Store Message in buffer
			 * 		4. Send message
			  */
			out_msg.tag = DATA_MSG;
			max_batch = msg_count + BATCH_SIZE;
			while ((msg_count <= num_packets) &&
					(!buffer_isFull(&message_buffer[me]))  &&
					(msg_count <= max_batch)) { 
				printdb("Sending DATA: LTS=%d\n", lts);
				lts++;
				out_msg.payload[0] = msg_count++;
				out_msg.payload[1] = lts; 
				out_msg.payload[2] = (lts+msg_count)*(me+2);
			
				max_mid[me]++;
				max_lts[me] = lts;
				buffer_append(&message_buffer[me], out_msg.payload[1], out_msg.payload[2]);
				sendto(ss,(char *) &out_msg, sizeof(Message), 0,
					   (struct sockaddr *)&send_addr, sizeof(send_addr));
			}
			
			/*  Send Lost messages  */
	/*		while (!lost_msg_list.isEmpty())  {
				out_msg = lost_msg_list.deq();
				sendto(ss,(char *) &out_msg, sizeof(Message), 0,
					   (struct sockaddr *)&send_addr, sizeof(send_addr));
			}
	*/			
		
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
		if (sock_num > 0 && FD_ISSET(sr, &temp_mask)) {
			from_len = sizeof(from_addr);

			/*** TODO:  change to recv_dbg()  ***/
			bytes = recvfrom(sr, mess_buf, sizeof(mess_buf), 0,
			                  (struct sockaddr *)&from_addr, &from_len );
			in_msg = (Message *) mess_buf;
			printdb("received some data TAG=%c, FROM=%d\n", in_msg->tag, in_msg->pid);
		} else {
			status_count = STATUS_TRIGGER;  /*  Always send STATUS MSG on timeout -- ??? */
			if (state == RECV)  {
				nak_count++;			/** TODO:  Determine NAK Handling  **/
				state == SEND;			
			}
			printdb("timed_out when trying to receive\n");
/*			continue;  */
		}

		from_pid = in_msg->pid;
		if (from_pid == me)  {
			continue;
		}
		status_count++;
		nak_count++;
		
		/*---------------------------------------------------------
		 * (3)  Implement RECV Algorithm
		 *--------------------------------------------------------*/
		switch (in_msg->tag)  {
			
			case GO_MSG:
				state = SEND;
				break;
		
			case DATA_MSG:
				/*  STORE DATA  & UPDATE MAX_MID RECV STATE */
				printdb("Received DATA MSG:\tLTS=%d,  VAL=%d\n", in_msg->payload[1], in_msg->payload[2]);
				if (in_msg->payload[1] > lts)  {
					lts = in_msg->payload[1];
				}
				/*  SET: highest in-order message ID from the last sender, "OPEN-1" 
				 /* DATA MESSAGE:
					*  payload[0] = message ID (mid)
					*  payload[1] = Lamport TimeStamp (LTS)
					*  payload[2] = Data Value  */
				max_mid[from_pid] = (buffer_put (&message_buffer[from_pid], in_msg->payload[1], in_msg->payload[2], in_msg->payload[0])) - 1;

				/*  SET: track LTS associated with that message ID */
				elm = buffer_get(&message_buffer[from_pid], max_mid[from_pid]);
				max_lts[from_pid] = elm->lts;
				printf(" MAX MID=");
				printarray(max_mid, num_machines);
				break;
			
			
			case STATUS_MSG:
				printdb("Received STATUS MSG:\tFROM:%d\n", from_pid);
				/*  Update SEND State  */
				if (in_msg->payload[me] > max_ack[from_pid])  {
					max_ack[from_pid] = in_msg->payload[me];
				}
				/*  CHECK LTS for message delivery  */
				printf(" MAX LTS=");
				printarray(max_lts, num_machines);
				
				deliver_lts = 10000001;
				for (index = 0; index < num_machines; index++)  {
					if (max_lts[index] < deliver_lts)  {
						deliver_lts = max_lts[index];
						printdb("NEW DELIVER LTS: %d\n", deliver_lts);
					}
				}

/*** ======================>>>>>  TODO HERE <<<================================  ***/

				/*  DELIVERY ALGORITHM  */
				if (deliver_lts >= 0)  {
					while (TRUE)  {
						cur_minlts = 10000000;
						for (index=0; index <num_machines; index++)  {
							if (max_lts[index] < 0)  {
								continue;
							}
							if (buffer_isEmpty(&message_buffer[index]))  {
								continue;
							}
							elm = buffer_get(&message_buffer[index], message_buffer[index].offset);
							
							if ((elm->lts < cur_minlts) && (elm->lts >= 0) && (elm->active == ACTIVE)) {
								cur_minlts = elm->lts;
								cur_minpid = index;
								/*printdb("WILL TRY TO DELIVER LTS: %d, PID: %d\n", cur_minlts, cur_minpid); */
							}
						}
						if (cur_minlts <= deliver_lts)  {
							printdb("DELIVERING:  LTS=%d,\tPID=%d\n", cur_minlts, cur_minpid);
							/*  WRITE HERE  */
							elm = buffer_get(&message_buffer[cur_minpid], message_buffer[cur_minpid].offset);
							fprintf(sink, "LTS: %d\t, %2d, %8d, %8d\n", elm->lts, cur_minpid, message_buffer[cur_minpid].offset, elm->data);
							buffer_clear(&message_buffer[cur_minpid], 1);
							/*exit(0);*/
						}
						else  {
							break;
						}
					}
				}
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
	fclose(sink);
	return 0;
}