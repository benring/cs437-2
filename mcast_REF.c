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
		printf(" %d,", v[i]);
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
	int				recv_count;
	int				max_batch;

	/*  Outgoing Message State (send)  */
	buffer			out_buffer;
	int				msg_count;
	int				num_packets;
	int				max_ack[MAX_MACHINES];			/* WHAT OTHER PEOPLE HAVE DELIVERED OF MY OUTGOING MESSAGE  */
	int 			send_window;
	int				lost_messages[MAX_MACHINES * NAK_QUOTA];
	int				num_lost_messages;

	
	/*  All Message State (receive)  */
	buffer			message_buffer[MAX_MACHINES];
	int				max_seq_num[MAX_MACHINES];
	int				max_lts[MAX_MACHINES];
	int				max_local_delivered[MAX_MACHINES];   /* WHAT I HAVE DELIVERED OF OTHER PEOPLE'S MESSAGES  */
	int				deliver_lts;
	int				have_delivered;
	int				mid[MAX_MACHINES];
	char			sending[MAX_MACHINES];
	int				all_inactive;
	int				all_received;
	
	
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
	int	i, j, k, loop;
	int				can_deliver;
	int				cur_minlts;
	int				cur_minpid;
	int				cur_minmid;


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
	num_packets = NUM_PACKETS_DEBUG;
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
	send_window = 0;
	num_lost_messages = 0;
	
	for (index=0; index < MAX_MACHINES; index++) {
		mid[index] = 0;
		max_seq_num[index] = -1;
		max_lts[index] = -1;
		max_ack[index] = -1;
		max_local_delivered[index] = -1;
		sending[index] = UNKNOWN;
	}
	if (num_packets > 0)  {
		sending[me] = ACTIVE;
	}
	else  {
		sending[me] = INACTIVE;
	}

    /*------------------------------------------------------------------
     *
     *      MAIN ALGORITHM EXECUTION
     *
     *------------------------------------------------------------------*/

	printdb("\n\n---------------------------------\n    MCAST: PID =  %d\n\n", me);

	for(loop=0; loop<100; loop++) {  
		/*---------------------------------------------------------
		 * (1)  Implement SEND Algorithm
		 *--------------------------------------------------------*/

		if (state == SEND)  {
			
			/******************************************
			 *   SEND STATUS Message
			 *****************************************/
			if (status_count >= STATUS_TRIGGER)  {
				
				printdb("STATUS TRIGGERED!\n");
				status_count = 0;
				out_msg.tag = STATUS_MSG;
				
				/* CHECK:  Has everyone completed sending messages  */
				all_inactive = TRUE;
				for (index=0; index<MAX_MACHINES; index++) {
					if (sending[index] == ACTIVE)  {
						all_inactive = FALSE;
					}
				}

				/*  IF no one is sending anything, shut down  */
				if (all_inactive)  {
					printdb("ALL INACTIVE\n");
					break;
				}
				
				
				/*  CHECK: Am I done sending Messages?  */
				if (msg_count > num_packets)  {
					printdb("END OF MESSAGES\n");
					sending[me] = INACTIVE;
				}

				/*  BUILD:  STATUS Message  */
				for (index=0; index<num_machines; index++) {
					out_msg.payload[index] = max_local_delivered[index];
					/*out_msg.payload[index+MAX_MACHINES] = max_local_delivered[index];*/
				}
				
				printdb(" My Message delivered for each PID: ");
				printarray(max_local_delivered, num_machines);
				printdb(" My Message Rec'd in order by PID:  ");
				printarray(max_seq_num, num_machines);
				printdb(" My Message ACK's by PID:           ");
				printarray(max_ack, num_machines);

				/*  SEND: Status Message */
				sendto(ss,(char *) &out_msg, sizeof(Message), 0,
					   (struct sockaddr *)&send_addr, sizeof(send_addr));
					   
			}

			/******************************************
			 *   SEND NAK Message
			 *****************************************/
			if (nak_count >= NAK_TRIGGER)  {
				printdb("NAK TRIGGERED!  BUILD NAK: ");

				nak_count = 0;
				out_msg.tag = NAK_MSG;
				
				/* CHECK NAK HEURISTIC  */
				for (index=0; index < num_machines; index++)  {
					out_msg.payload[index * NAK_QUOTA] = 0;
					if (index == me)  {
						continue;
					}
					printdb(" / PID-%d [", index);
					j = 0;
					
					if (buffer_end(&message_buffer[index]) > message_buffer[index].open) {
						printdb("THERE SHOULD BE A LOST PACKET:");
						for (i=message_buffer[index].offset; i < buffer_end(&message_buffer[index]); i++) {
/*						while ((i < NAK_QUOTA) && (i < buffer_size(&message_buffer[index]))) {  */
							elm = buffer_get(&message_buffer[index], i);
							if (elm->active == INACTIVE) {
								printdb("FOUND A LOST PACKET!!!!!!\n");
								j++;
								out_msg.payload[index * NAK_QUOTA]++;
								out_msg.payload[(index * NAK_QUOTA) + j] = i;
								printdb(" Msg#%d, ", out_msg.payload[(index * NAK_QUOTA) + j]);
								if (j >= NAK_QUOTA)  {
									break;
								}
							}
						}
					}
					printdb("]");
				}
				printdb("\n");
				/*  SEND NAK MESSAGE  */
				sendto(ss,(char *) &out_msg, sizeof(Message), 0,
					   (struct sockaddr *)&send_addr, sizeof(send_addr));
			}
			
			/*  Sending Algorithm:
			 * 		1. Create New Packets (if possible)
			 * 		2. Increment & Stamp LTS
			 * 		3. Store Message in buffer
			 * 		4. Send message
			  */

			
			/******************************************
			 *   SEND DATA Message(s)
			 *****************************************/
			if (state == SEND)  {
				out_msg.tag = DATA_MSG;

				/*  Send a "batch" of messages  */
				max_batch = msg_count + BATCH_SIZE;

				/*  Ensure I do not send messages if someone else's buffer for my message is full  */
				send_window = 100000000;
				for (index=0; index<num_machines; index++)  {
					if (max_local_delivered[index] < send_window)  {
						send_window = max_local_delivered[index];
					}
				}
				send_window += MAX_BUFFER_SIZE;
				printdb(" Try to SEND:  msg_cnt=%d, send_win=%d\n", msg_count, send_window); 
				  
				/*  SEND a message under certain conditions  */
				while ( (msg_count <= num_packets) && 
						(msg_count < max_batch) &&
						(msg_count < send_window) &&    
						(!buffer_isFull(&out_buffer)) && 
						(!buffer_isFull(&message_buffer[me]))  ) { 

					/*  Increment the LTS  */
					lts++;
					
					/*  Create & Prepare the message  */
					printdb("Sending DATA: MsgID=%d, LTS=%d\n", msg_count, lts);
					out_msg.payload[0] = msg_count;
					out_msg.payload[1] = lts; 
					out_msg.payload[2] = (lts+msg_count)*(me+2);
				
					/*  Update local state, to include the "receiving" state for my message */
					msg_count++;
					max_seq_num[me]++;
					max_ack[me]++;
					max_lts[me] = lts;

					/*  Store message in both outgoint & in-coming (for receiving) buffers */
					buffer_append(&message_buffer[me], out_msg.payload[1], out_msg.payload[2]);
					buffer_append(&out_buffer, out_msg.payload[1], out_msg.payload[2]);

					/*  SEND: Data Message */
					sendto(ss,(char *) &out_msg, sizeof(Message), 0,
						   (struct sockaddr *)&send_addr, sizeof(send_addr));
				}
			}
			
			/*  Send Lost messages  */
			if (num_lost_messages > 0)  {
				printdb("RESEND LOST Messages-> ");
				for (index=0; index < num_lost_messages; index++)  {
					elm = buffer_get(&out_buffer, lost_messages[index]);
					out_msg.payload[0] = lost_messages[index];
					out_msg.payload[1] = elm->lts; 
					out_msg.payload[2] = elm->data;
					printdb(" Msg#%d", lost_messages[index]);
					sendto(ss,(char *) &out_msg, sizeof(Message), 0,
						   (struct sockaddr *)&send_addr, sizeof(send_addr));
				}
				printdb("\n");
				num_lost_messages = 0;
			}
				
		
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
/*			printdb("received some data TAG=%c, FROM=%d", in_msg->tag, in_msg->pid);  */
		} else {
			status_count = STATUS_TRIGGER;  /*  Always send STATUS MSG on timeout -- ??? */
			if (state == RECV)  {
				nak_count++;			/** TODO:  Determine NAK Handling  **/
				state == SEND;			
			}
/*			continue;  */
		}

		from_pid = in_msg->pid;
		
		/*  IGNORE: My sent messages  */
		if (from_pid == me)  {
			continue;
		}
		
		status_count++;
		nak_count++;
		recv_count++;
		
		/*---------------------------------------------------------
		 * (3)  Implement RECV Algorithm
		 *--------------------------------------------------------*/
		switch (in_msg->tag)  {
			
			/****************************************************
			 *  RECEIVE the Start Signal to get out of IDLE State
			 *****************************************************/
			case GO_MSG:
				state = SEND;
				break;
		
			/****************************************************
			 *  RECEIVE a data message
			 *****************************************************/
			case DATA_MSG:
				/*  STORE DATA  & UPDATE max_seq_num RECV STATE */
				printdb("Received DATA MSG:  PID=%d, MsgID=%d, LTS=%d\n", from_pid, in_msg->payload[0], in_msg->payload[1]);
				if (in_msg->payload[1] > lts)  {
					lts = in_msg->payload[1];
				}
				/*  SET: highest in-order message ID from the last sender, "OPEN-1" 
				 /* DATA MESSAGE:
					*  payload[0] = message ID (mid)
					*  payload[1] = Lamport TimeStamp (LTS)
					*  payload[2] = Data Value  */
				if (buffer_isFull(&message_buffer[from_pid]))  {
					printdb("******Ignoring Message due to full buffer on %d ******\n", from_pid);
					/*exit(0);*/
				}
				else {
					
					/*  UPDATE: Set highest in-order message ID from this sender  */
/*					printf("     LTS Vector (pre)=");
					printarray(max_lts, num_machines);
*/	
					printdb("Puttin A...\n");
					max_seq_num[from_pid] = (buffer_put (&message_buffer[from_pid], in_msg->payload[1], in_msg->payload[2], in_msg->payload[0])) - 1;
					printdb("Puttin B...\n");
					if (buffer_isFull(&message_buffer[from_pid]))  {
						printdb("  ******MY BUFFER IS NOW FULL FOR PID = %d ******\n", from_pid);
						
					}
					printdb("Puttin C...\n");

					/*  UPDATE: set LTS associated with the highest in-order message ID  from this sender */
					elm = buffer_get(&message_buffer[from_pid], max_seq_num[from_pid]);
					printdb("Puttin D...\n");
					max_lts[from_pid] = elm->lts;
/*					printf("     LTS Vector (post)=");
					printarray(max_lts, num_machines);
*/
					printdb("  STORING Message. High MAX-SEQ_NUM for this pid= %d, lts=%d\n", max_seq_num[from_pid], max_lts[from_pid]);
				}
				
	
				/*************************
				/*  DELIVERY ALGORITHM  *
				 * ***********************/
/*				printf(" LTS Vector=\t");
				printarray(max_lts, num_machines);
*/
				/*  STEP #1: Find MIN "highest in-order LTS" among all sending processes */
				deliver_lts = 10000001;
				for (index = 0; index < num_machines; index++)  {
					if (sending[index] == UNKNOWN)  {
						printdb("UNKNOWN STATUS FROM pid-%d\n", index);
						deliver_lts = -1;
						break;
					}
					if ((max_lts[index] < deliver_lts) && (sending[index] == ACTIVE)) {
						deliver_lts = max_lts[index];
					}
				}
				printdb("Try to deliver up to LTS= %d\n", deliver_lts);
				printf("  SN Vector=");
				printarray(max_seq_num, num_machines);
				printf("  LTS Vector=");
				printarray(max_lts, num_machines);

				/*  STEP #2: Traverse Message Buffers & iteratively find & deliver lowest LTS (& lowest PID) up to the "deliver_LTS"  */
				if (deliver_lts >= 0)  {
					while (TRUE)  {
						
						/*  FIND: lowest LTS for all messages we've seen */
						cur_minlts = 10000000;
						for (index=0; index <num_machines; index++)  {
							
							/* Ignore any LTS of -1 (= this PID is not sending messages)  */
							if (max_lts[index] < 0)  {
								continue;
							}
							
							/* Ignore empty message buffers (haven't received any messages */
							if (buffer_isEmpty(&message_buffer[index]))  {
								printdb("EMPTY BUFFER for pid#%d\n", index);
								continue;
							}

							/* get MIN LTS for all sending processes */
							elm = buffer_get(&message_buffer[index], message_buffer[index].offset);
							if ((elm->lts < cur_minlts) && (elm->lts >= 0) && (elm->active == ACTIVE)) {
								cur_minlts = elm->lts;
								cur_minpid = index;
								/*printdb("WILL TRY TO DELIVER LTS: %d, PID: %d\n", cur_minlts, cur_minpid); */
							}
						}
						
/*						if (cur_minpid == me)  {
							i = 1000000000;
							for (index = 0; index < num_machines; index++)   {
								if (index == me) {
									continue;
								}
								if (max_local_delivered[index] < i) {
									i = max_local_delivered[index];
								}
							}
							if (message_buffer[cur_minpid].offset > i) {
								break;
							}
						}
*/						
						/*  CHECK:  Can we deliver the min LTS message? */
						if (cur_minlts <= deliver_lts)  {
							printdb("DELIVERING:  LTS=%d,\tPID=%d\n", cur_minlts, cur_minpid);
							elm = buffer_get(&message_buffer[cur_minpid], message_buffer[cur_minpid].offset);
							fprintf(sink, "LTS: %d\t, %2d, %8d, %8d\n", elm->lts, cur_minpid, message_buffer[cur_minpid].offset, elm->data);
							
							/*  Clean up the buffer  */
							buffer_clear(&message_buffer[cur_minpid], 1);
							max_local_delivered[cur_minpid]++;
							/*exit(0);*/
						}
						else  {
							break;
						}
					}
				}
				 /*  END DELIVERY ALGORITHM  */			
				
				break;
			
			/****************************************************
			 *  RECEIVE a STATUS message
			 *****************************************************/
			case STATUS_MSG:
				/*  Update SEND State  */
/*				if (in_msg->payload[me] > max_ack[from_pid])  {
					max_ack[from_pid] = in_msg->payload[me];
				}
*/				
				if (in_msg->payload[me] > max_ack[from_pid]) {
					max_ack[from_pid] = in_msg->payload[me];
				}

				printdb("Received STATUS MSG FROM:%d\tMAX_ACK:%d\n", from_pid, max_ack[from_pid]);
				/*  UPDATE: Local State for each PID  */
				cur_minmid = 100000000;
				all_received = TRUE;
				
				/*  CHECK: Is this process sending a message */
				if (in_msg->payload[from_pid] >= 0) {
					sending[from_pid] = ACTIVE;
				}
				else {
					sending[from_pid] = INACTIVE;
				}
				
				for (index=0; index < num_machines; index++)  {
					/*  CHECK: Are processes still awaiting messages from me? */
					if (max_ack[index] < num_packets) {
						all_received = FALSE;
					}
					/*  DETERMINE: What is the lowest MID I can flush  -- BASED ON MAX ACK'd */
					if (max_ack[index] < cur_minmid)  {
						cur_minmid = max_ack[index];
					}
				}
				
				/*  CHECK: If everyone has received all my message, I am no longer a sender  */
				if (all_received)  {
					sending[me] = INACTIVE;
				}

				/*   CHECK:  Flush out-going message buffer  */
/*				printf(" MAX DELIVERED:\t");
				printarray(max_local_delivered, num_machines);
*/				
				printdb("  FLUSH ??:  min MID=%d, MyOffset=%d\n",  cur_minmid, out_buffer.offset);
				if (cur_minmid >= out_buffer.offset) {
					buffer_clear(&out_buffer, (cur_minmid - out_buffer.offset + 1));   /*  ************ CHECK OFF BY 1  *******/
				}
				printdb("  FLUSH TO:  min MID=%d, MyOffset=%d\n",  cur_minmid, out_buffer.offset);
				break;
				
			/****************************************************
			 *  RECEIVE a NAK message
			 *****************************************************/
			case NAK_MSG:
				/*  Update LOST msg queue  */
				printdb("------Received NAK from %d: ------------------- ", from_pid);

				for(index=0; index<in_msg->payload[me*NAK_QUOTA]; index++)  {
						lost_messages[num_lost_messages] = in_msg->payload[(me*NAK_QUOTA) + index + 1];
						printdb("Msg#%d ", lost_messages[num_lost_messages]);
						num_lost_messages++;
				}
				printdb(" -- total=%d\n", num_lost_messages);
				break;
			
			case EOM_MSG:
				/*  Process EOM Message  */
				sending[from_pid] = INACTIVE;
				break;
				
			default:
				printdb("Process Received erroneous message");
		}
		
		state = SEND;

	} 
	fclose(sink);
	return 0;
}