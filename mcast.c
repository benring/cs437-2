/****************************************************************************
 * File: mcast.c
 *
 * Author: Benjamin Ring & Josh Wheeler
 * Description:  The mutlicast process 
 *
 * *************************************************************************/
#include "recv_dbg.h"
#include "config.h"
#include "message.h"
#include "buffer.h"
#include "linkedlist.h"

/****************************************
 *   MAIN PROGRAM EXECUTION  
 ****************************************/

void printarray(int *v, int n)  {
	int i;
	for (i=0; i<n; i++)  {
		printf("  %3d,", v[i]);
	}
	printf("\n");
}

void print_line() {
  printdb("-----------------------\n");

}
void print_buffers(buffer *b, int num_machines) {
  int i;
  for(i=0; i < num_machines; i++) {
    print_line();
    printdb("P%d:\n", i);
    buffer_print(&b[i]);
    print_line();
  }

}

int get_min(int * a, int num)  {
	int i;
	int min;
	min = a[0];
	for (i=1; i<num; i++) {
		if (a[i] < min) 
			min = a[i];
	}
	return min;
}


int get_min_exclude(int * a, int num, int exclude)  {
	int i;
	int min;
	min = 1000000000;
	for (i=0; i<num; i++) {
		if (i == exclude)  {
			continue;
		}
		if (a[i] < min) 
			min = a[i];
	}
	return min;
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
/*	socklen_t             from_len;  */
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
	char			sending[MAX_MACHINES];   

	
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
	int				loop, i, j;
	int				can_deliver;
	int				cur_minlts;
	int				cur_minpid;
	int				send_flag;
	int				sequence_num;
	int				send_window;
	int				all_inactive;


        linked_list *                   lost_packets;
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
	num_packets = NUM_PACKET_DEBUG;
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
	
	recv_dbg_init(5, me);  

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
		message_buffer[index] = buffer_init();
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



	printdb("\n\n\n-------------------------------------------------------------------------------------\n    MCAST: PID =  %d\n\n", me);

	for(loop=0; loop<1000; loop++) {  
		/*---------------------------------------------------------
		 * (1)  Implement SEND Algorithm
		 *--------------------------------------------------------*/

		if (state != IDLE)  {
			
			/******************************************
			 *   SEND STATUS Message
			 *****************************************/
			if (status_count >= STATUS_TRIGGER)  {

				/* STATUS CHECK:  Has everyone completed sending messages  */
				all_inactive = TRUE;
				for (index=0; index<MAX_MACHINES; index++) {
					if (sending[index] == ACTIVE)  {
						all_inactive = FALSE;
					}
				}

				/*  IF no one is sending anything, shut down  */
				if (all_inactive)  {
					printdb("ALL INACTIVE -- GOING TO KILL STATE\n");
					state = KILL;
					break;
				}
				
				
				status_count = 0;
				out_msg.tag = STATUS_MSG;
				for (index=0; index<MAX_MACHINES; index++) {
					out_msg.payload[index] = max_mid[index];
				}
				printdb("SENDING STATUS Msg: Max-Mid= ");
				printarray(max_mid, num_machines);
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
				for(index = 0; index < num_machines; index++) {
				  out_msg.payload[index*NAK_QUOTA] = 0;
				  if (index == me) {
						continue;
				  }
					printdb(" / PID-%d [", index);
					j = 1;
					
					printdb("A%d=%d,", message_buffer[index].upperlimit, buffer_first_open(&message_buffer[index]));
					if (message_buffer[index].upperlimit > buffer_first_open(&message_buffer[index])+1) {
						for (i=message_buffer[index].offset; i <= message_buffer[index].upperlimit; i++) {
						printdb("B");
							/*elm = buffer_get(&message_buffer[index], i);*/
							if (!buffer_isActive(&message_buffer[index], i)) {
								nak_count++;
								out_msg.payload[index * NAK_QUOTA]++;
								out_msg.payload[(index * NAK_QUOTA) + j] = i;
								printdb(" Msg#%d, ", i);
								j++;
								if (j == NAK_QUOTA)  {
									break;
								}
							}
						}
					}
					printdb("]");

/*				  out_msg.payload[index*NAK_QUOTA+1] = message_buffer[index].offset;  
				  printf("NAK for pid %d, SN %d\n", index, message_buffer[index].offset);
*/
				}
				printdb("\n");
				/*  SEND NAK MESSAGE  */
				if (nak_count > 0)  {
					printdb("SENDING NAK for %d lost messages!\n", nak_count); 
					sendto(ss,(char *) &out_msg, sizeof(Message), 0,
					   (struct sockaddr *)&send_addr, sizeof(send_addr));
				}
				nak_count = 0;
			}
			

			/******************************************
			 *   SEND DATA Message
			 *****************************************/
			/*  Sending Algorithm:
			 * 		1. Create New Packets (if possible)
			 * 		2. Increment & Stamp LTS
			 * 		3. Store Message in buffer
			 * 		4. Send message
			  */
			out_msg.tag = DATA_MSG;
			max_batch = msg_count + BATCH_SIZE;
			send_flag = FALSE;
			
				send_window = 100000000;
				for (index=0; index<num_machines; index++)  {
					if ((index == me) && (max_mid[me] <0))  {
						continue;
					}
					if (max_mid[index] < send_window)  {
						send_window = max_mid[index];
					}
				}

			send_window += MAX_BUFFER_SIZE;
			printdb("MAX SEND:  %d   \n MAX-MID= ", send_window);
			printarray(max_mid, num_machines);
			while ( (msg_count <= num_packets) &&
					(msg_count < max_batch)  &&
					(msg_count <= send_window) &&
					(!buffer_isFull(&message_buffer[me])) ) {
				send_flag = TRUE;
				
				lts++;

				printdb("Sending DATA MSG #%d   (LTS=%d)\n", msg_count, lts);

				 /* DATA MESSAGE:
					*  payload[0] = message ID (mid)
					*  payload[1] = Lamport TimeStamp (LTS)
					*  payload[2] = Data Value  */
				out_msg.payload[0] = msg_count++;
				out_msg.payload[1] = lts; 
				out_msg.payload[2] = (lts+msg_count)*(me+2);
			
				max_mid[me]++;
				max_lts[me] = lts;
				buffer_append(&message_buffer[me], out_msg.payload[1], out_msg.payload[2]);
				sendto(ss,(char *) &out_msg, sizeof(Message), 0,
					   (struct sockaddr *)&send_addr, sizeof(send_addr));
				have_delivered++;
			}

/*			if (send_flag)  {
				printdb("SEND SOME MESSAGES\n");
				print_buffers(message_buffer, num_machines);
			}
*/		

			/******************************************
			 *   RE-SEND Lost Data Message(s)
			 *****************************************/
			while(!isEmpty(lost_packets)) {
			  index = deq(&lost_packets);
			  printdb("NEED to RE-SEND %d: ", index);
			  /* Ensure packet is is my window  */
			  if ((index >= message_buffer[me].offset) && (index < buffer_end(&message_buffer[me])) ) {
				printf("Resending packet %d\n", index);
				out_msg.tag = DATA_MSG;
				elm = buffer_get(&message_buffer[me], index);
				out_msg.payload[0] = index;
				out_msg.payload[1] = elm->lts;
				out_msg.payload[2] = elm->data;
				printdb("Re-Sending DATA MSG #%d   (LTS=%d)\n", index, elm->lts);
				sendto(ss,(char *) &out_msg, sizeof(Message), 0,
					   (struct sockaddr *)&send_addr, sizeof(send_addr));
				}
            }
			printdb("\n");

		
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
/*			from_len = sizeof(from_addr);  */
			bytes = (state == IDLE) ?
				recv(sr, mess_buf, sizeof(mess_buf), 0) :
				recv_dbg(sr, mess_buf, sizeof(mess_buf), 0);
			if (bytes == 0) {
			  printdb("DROPPED A PACKET\n");
			  continue;
			}
			in_msg = (Message *) mess_buf;
		} else {
			status_count = STATUS_TRIGGER;  /*  Always send STATUS MSG on timeout -- ??? */
			nak_count = NAK_TRIGGER;
			if (state == RECV)  {
				state == SEND;			
			}
			printdb("timed_out when trying to receive\n");
			continue;
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
			
			/****************************************************
			 *  RECEIVE the Start Signal to get out of IDLE State
			 *****************************************************/
			case GO_MSG:
				state = SEND;
				break;
		
			/****************************************************
			 *  RECEIVE a DATA message
			 *****************************************************/
			case DATA_MSG:
				/*  STORE DATA  & UPDATE MAX_MID RECV STATE */
				printdb("Received DATA MSG from PID-%d, MID=%d, LTS=%d\n", from_pid, in_msg->payload[0], in_msg->payload[1]);
				print_buffers(message_buffer, num_machines);
				if (in_msg->payload[1] > lts)  {
					lts = in_msg->payload[1];
				}
				/*  SET: highest in-order message ID from the last sender, "OPEN-1" 
				 /* DATA MESSAGE:
					*  payload[0] = message ID (mid)
					*  payload[1] = Lamport TimeStamp (LTS)
					*  payload[2] = Data Value  */

					/*  CHECK: Duplicate packet  */
					if (buffer_isActive(&message_buffer[from_pid], in_msg->payload[0])) {
						printdb("  **Received DUPLICATE Packet.\n");
					}
					/*  CHECK: Old Packet */
					else if (in_msg->payload[0] < message_buffer[from_pid].offset) {
						printdb("RECEIVED AN OLD PACKET\n");
					}
					/*  CHECK: INDEX OOB  -- TODO:  Handle this better*/
					else if (in_msg->payload[0] > (message_buffer[from_pid].offset + MAX_BUFFER_SIZE)) {
						printdb("******Ignoring Message due to lack of buffer size on %d ******\n", from_pid);
					}
					/*  CHECK:  Buffer Overflow  */
					else if (buffer_isFull(&message_buffer[from_pid]))  {
						printdb("******Ignoring Message to due to Buffer Overflow on %d ******\n", from_pid);
						/*exit(0);*/
					} 
					else {
					
						sequence_num = (buffer_put (&message_buffer[from_pid], in_msg->payload[1], in_msg->payload[2], in_msg->payload[0])) - 1;

						/*  SET: track LTS associated with that message ID */
						elm = buffer_get(&message_buffer[from_pid], sequence_num);
						max_lts[from_pid] = elm->lts;
						
						print_buffers(message_buffer, num_machines);
					}
				
				break;
			
			
			/****************************************************
			 *  RECEIVE a STATUS message
			 *****************************************************/
			case STATUS_MSG:
				printdb("Received STATUS MSG from PID-%d\n", from_pid);
				/*  Update SEND State  */
				if (in_msg->payload[me] > max_ack[from_pid])  {
					max_ack[from_pid] = in_msg->payload[me];
				}

				/*  CHECK: Am I done sending Messages?  */
				
				printdb(" MAX ACK = ");
				printarray(max_ack, num_machines);
				if (get_min_exclude(max_ack, num_machines, me) == num_packets)  {
					printdb("END OF MESSAGES  ------------- GOING INACTIVE\n");
					sending[me] = INACTIVE;
				}
				
				/*  CHECK: Is this process sending a message */
				if (in_msg->payload[from_pid] >= 0) {
					sending[from_pid] = ACTIVE;
				}
				else {
					printdb("PROCESS %d HAS GONE INACTIVE\n");
					sending[from_pid] = INACTIVE;
				}
				

				
				/****************************************************
				 *  DELIVERY ALGORITHM 
				 *****************************************************/
				/*  CHECK LTS for message delivery  */
				printf(" MAX LTS=");
				printarray(max_lts, num_machines);
				
				deliver_lts = 10000001;
				for (index = 0; index < num_machines; index++)  {
					if (sending[index] == UNKNOWN)  {
						printdb("UNKNOWN STATUS FROM pid-%d\n", index);
						deliver_lts = -1;
						break;
					}
					if ((max_lts[index] < deliver_lts) && (sending[index] == ACTIVE))  {
						deliver_lts = max_lts[index];
						printdb("Check DELIVER thru LTS: %d (this active pid=%d)\n", deliver_lts, index);
					}
				}



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
						if ((cur_minpid == me) && (get_min_exclude(max_ack, num_machines, me) < message_buffer[me].offset)) {
							break;
						}
						if (cur_minlts <= deliver_lts)  {
							printdb("DELIVERING:  LTS=%d,PID=%d\n", cur_minlts, cur_minpid);
							/*  WRITE HERE  */
							elm = buffer_get(&message_buffer[cur_minpid], message_buffer[cur_minpid].offset);
							fprintf(sink, "LTS: %d\t, %2d, %8d, %8d\n", elm->lts, cur_minpid, message_buffer[cur_minpid].offset, elm->data);
	
							/*  Set MAX_MID to what you are about to deliver  */
							printdb("MaxMid Array = "); printarray(max_mid, num_machines);
							max_mid[cur_minpid] = message_buffer[cur_minpid].offset;
							printdb("MaxMid Array = "); printarray(max_mid, num_machines);
							buffer_clear(&message_buffer[cur_minpid], 1);
							
							/*  If you have delivered your last message, reset your max-mid to -1  */
							if ((cur_minpid == me) && (max_mid[me] == num_packets)) {
								max_mid[me] = -1;
							}
							/*exit(0);*/
						}
						else  {
							break;
						}
					}
					print_buffers(message_buffer, num_machines);
					
				}
				break;
				
			
			/****************************************************
			 *  RECEIVE a NAK message
			 *****************************************************/
			case NAK_MSG:
				/*  Update LOST msg queue  */
				printdb("------Received NAK from %d for: ", from_pid);
				
				index = me*NAK_QUOTA;
				i = 1;
				while (i <= in_msg->payload[index])  {
					j = in_msg->payload[index + i];
					i++;
					printdb(" Msg#%d", j);
					if (j >= have_delivered) {
						printdb("<Ignore%d >= %d>", j, buffer_end(&message_buffer[me]));
					}
					else {
						printdb("<Enq>");
						enq(&lost_packets, j);
					}
				}
				printdb("\n");

/*				num = in_msg->payload[me*NAK_QUOTA];
				printdb("I need to resend %d packets \n", num);
				printdb("First one is: %d\n", in_msg->payload[me*NAK_QUOTA+1]);
				enq(&lost_packets, in_msg->payload[me*NAK_QUOTA+1]);
*/				break;
				
			
			case KILL_MSG:
				state = KILL;
				break;
				
			default:
				printdb("Process Received erroneous message");
		}

		if (state == KILL)  {
			break;
		}

	} 
	fclose(sink);
	
	if (state != KILL)  {
		printdb("Program ended prematurely\n");
		return(1);
	}
	else {
		out_msg.tag = KILL_MSG;
		for (index = 0; index < num_machines; index++)  {
			printdb("SEND: KILL Code #%d\n", index);
			sendto(ss,(char *) &out_msg, sizeof(Message), 0,
			   (struct sockaddr *)&send_addr, sizeof(send_addr));
					   
		}
		printdb("\n Terminating.\n-------------------------------\n\n");
	}
	return 0;
}
