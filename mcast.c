/****************************************************************************
 * File: mcast.c
 *
 * Author: Benjamin Ring & Josh Wheeler
 * Description:  The Reliable mutlicast process 
 *
 * *************************************************************************/
#include "recv_dbg.h"
#include "config.h"
#include "buffer.h"
#include "linkedlist.h"

/****************************************
 *   MAIN PROGRAM EXECUTION  
 ****************************************/

/*  Print functions for displaying debugging information */
void printarray(int *v, int n);
void printstatuses (int * v, int n);
void print_line();
void print_buffers(buffer *b, int num_machines);
void print_buffers_select(buffer *b, int num_machines);
int get_min(int * a, int num);

/*  Other Helper functions */
float time_diff (struct timeval s, struct timeval e);
int main(int argc, char*argv[])
{

    /*----------------------------------------
     * VAR DECLARATIONS
     *----------------------------------------*/

	/* Networking vars */
	struct sockaddr_in    recv_addr;
	struct sockaddr_in    send_addr;
	struct sockaddr_in    resend_addr;
	struct sockaddr_in    from_addr;
	socklen_t             from_len;
	int						from_ip;
	struct ip_mreq			mreq;
	unsigned char			ttl_val;
	int                   ss,sr, su;
	fd_set                mask;
	fd_set                dummy_mask,temp_mask;
	int                   bytes;
	int                 	sock_num;
	char					one_mess[MAX_PACKET_SIZE];
	char                	mess_buf[MAX_PACKET_SIZE][BATCH_RECEIVE];
	struct timeval  		timeout;        

	/* Message structs for interpreting bytes */
	Message         out_msg;
	Message			*in_msg;
	Value			*elm;
	list_elm		packet;

	/* Process's Internal State */
	int				state;
	int				me;
	int				num_machines;
	int				lts;
	int				status_count;
	int				nak_count;
	int 			resend_count;
	int				receive_count;
	int				receive_amount;
	int				send_count;
	int				send_amount;
	int				max_send;
	int				loss_rate;
	int				all_inactive;
	int				all_delivered;
	int				host_list[MAX_MACHINES];

	/*  Outgoing Message State (send)  */
	buffer			out_buffer;
	linked_list *   lost_packets;
	int				msg_count;
	int				num_packets;
	int				max_ack[MAX_MACHINES];
	int				max_delivered[MAX_MACHINES];
	int				last_nak[MAX_MACHINES];
	int				last_nak_count[MAX_MACHINES];
	int				this_nak;

	/*  All Message State (receive)  */
	buffer			message_buffer[MAX_MACHINES];
	linked_list *   overflow[MAX_MACHINES];
	int 			overflow_size[MAX_MACHINES];
	int				max_mid[MAX_MACHINES];
	int				max_lts[MAX_MACHINES];
	int				last_message[MAX_MACHINES];
	int				deliver_lts;
	int				have_sent;
	int				sending[MAX_MACHINES];   
	
	/*  File I/O vals  */
	FILE            *sink;
	char            dest_file[10];

	/*  Packet Handling  */
	int				from_pid;

	/*  Other vars	*/
	int				index;
	int				loop, i, j;
	int				cur_minlts;
	int				cur_minpid;
	int				cur_minmid;
	int				cur_maxack;
	int				cur_maxdel;
	int				num_naks;
	int				my_nakpos;
	int				sequence_num;
	int				store_count;
	int				display_count;
	int 			display_interval;
	struct timeval  start_t;
	struct timeval  end_t;

	/*   Optimization vars  */
	int				store;
	int				dup;
	int				old;
	int				resend;
	int				naklast;
	int				oob;
	int				full;
	int				to;
	long			totime;
	long			waitrecvtime;
	int				numnak;
	int				naktrig;
	int				naksent;
	int				lessfull;
	int				morefull;
	struct timeval  d1;
	struct timeval  d2;
	float			ddiff = 0.0;
	float			naksenddiff = 0.0;
	float			nakrecvdiff = 0.0;
	



	/*------------------------------------------------------------
	*    (1)  Conduct program Initialization
	* --------------------------------------------------------------- */

	/*  Parse Command Line Args   */
	if (argc != 5) {
		printf("usage: %s <num_of_packets> <machine_index> <num_of_machines> <loss_rate>\n", argv[0]);
		exit(-1);
	}
	num_packets = atoi(argv[1]);
	me =  atoi(argv[2]) - 1;  
	num_machines = atoi(argv[3]);
	loss_rate = atoi(argv[4]);
	sprintf(dest_file, "%d.out", (me+1));
	
	recv_dbg_init(loss_rate, me);  
	srand(time(NULL));
	
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
	recv_addr.sin_port = htons(PORT);
	from_len = sizeof(from_addr);
	if ( bind( sr, (struct sockaddr *)&recv_addr, sizeof(recv_addr) ) < 0 ) {
		perror("Mcast: bind");
		exit(1);
	}

	mreq.imr_multiaddr.s_addr = htonl(MCAST_ADDR);
	mreq.imr_interface.s_addr = INADDR_ANY;
	if (setsockopt(sr, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
		(void *)&mreq, sizeof(mreq))<0)  {
			perror("Mcast: problem with setting sock option");
		}
		
	/* Set up socket for sending multicast */
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

	/*  Set up Unicast Socket for re-sending data packets */
		/* Open socket for sending */
	su = socket(AF_INET, SOCK_DGRAM, 0); /* socket for sending (udp) */
	if (su<0) {
		perror("Ucast: socket");
		exit(1);
	}

	resend_addr.sin_family = AF_INET;
	resend_addr.sin_port = htons(PORT);

	FD_ZERO( &mask );
	FD_ZERO( &dummy_mask );
	FD_SET( sr, &mask );


	/*  Initialize Program State Variables  */
	state = IDLE;
	lts = -1;
	deliver_lts = -1;
	have_sent = -1;
	msg_count = 0;
	status_count = 0;
	nak_count = 0;
	resend_count = 0;
	out_msg.pid = me;
	send_amount = me;
	receive_count = 0;
	receive_amount = num_machines - me;
	out_buffer = buffer_init(2*(MAX_BUFFER_SIZE));
	display_interval = num_packets / 10;
	display_count = display_interval;
	store_count = display_interval;
	cur_minpid = 0;

	
	for (index=0; index < MAX_MACHINES; index++) {
		max_mid[index] = -1;
		max_lts[index] = -1;
		max_ack[index] = -1;
		max_delivered[index] = -1;
		last_nak[index] = 0;
		last_nak_count[index] = 0;
		message_buffer[index] = buffer_init(MAX_BUFFER_SIZE);
		sending[index] = UNKNOWN;
		last_message[index] = -1;
		overflow_size[index] = 0;
		host_list[index] = 0;
	}

	/*  Initialize Optimization Variables  */
	dup=0;
	store=0;
	resend=0;
	oob=0;
	naklast=0;
	to=0;
	totime=0;
	waitrecvtime=0;
	full=0;
	old=0;
	numnak=0;
	naktrig=0;
	naksent=0;
	morefull=0;
	lessfull=0;

	/*  STATE CHANGE:  UNKNOWN to ACTIVE or INACTIVE */
	if (num_packets > 0)  {
		sending[me] = ACTIVE;
		max_mid[me] = 0;
	}
	else  {
		sending[me] = INACTIVE;
	}

    /*------------------------------------------------------------------
     *
     *      MAIN ALGORITHM EXECUTION
     *
     *------------------------------------------------------------------*/

	printinfo("\n\n\n-----------------------------------------\n");
	printinfo("			PID  =  %4d\n", (me+1));
	printinfo("			Loss =  %4d%%\n\n", loss_rate);
	

	for(loop=0; loop<10000000; loop++) {  
		/*==========================================================
		 * (1)  SEND STATE Algorithm Implementation
		 *==========================================================*/
		status_count++;
		nak_count++;

		if (state == SEND)  {
			resend_count++;

			/******************************************
			 *   SEND STATUS Message
			 *****************************************/
			if (status_count >= STATUS_TRIGGER)  {
				status_count = 0;
				out_msg.tag = STATUS_MSG;
				for (index=0; index<MAX_MACHINES; index++) {
					out_msg.payload[index] = buffer_first_open(&message_buffer[index])-1;
					out_msg.payload[1+index+MAX_MACHINES] = sending[index];
					out_msg.payload[1+index+(2*MAX_MACHINES)] = message_buffer[index].offset-1;
				}
				if (have_sent >= num_packets-1)  {
					printdb("I've DELIVERED my last packet. # Packet status is now %d\n", have_sent);
					out_msg.payload[MAX_MACHINES] = have_sent;
				}
				else {
					out_msg.payload[MAX_MACHINES] = -1;
				}
				printdb("SENDING STATUS Msg: Max-Mid= ");
				sendto(ss,(char *) &out_msg, sizeof(Message), 0,
					   (struct sockaddr *)&send_addr, sizeof(send_addr));
			}

			/******************************************
			 *   SEND NAK Message
			 *****************************************/
			if (nak_count >= NAK_TRIGGER)  {
				naktrig++;
				gettimeofday(&d1, NULL);
				printsel("NAK TRIGGERED!  BUILD NAK: ");
				nak_count = 0;
				out_msg.tag = NAK_MSG;
				
				/*---------------------------------------
				 *  NAK HEURISTIC ALGORITHM            
				 *---------------------------------------*/
				/*  CHECK: Overflow buffer  */
				for (index=0; index<num_machines; index++)  { 
					if ((index == me) || (overflow_size[index] == 0))  {
						continue;
					} 
					printdb("CHECK overflow on %d: ", index);
					for (i=0; i<overflow_size[index]; i++)  {
						packet = deq(&overflow[index]);
						printdb("%d", packet.mid);
						if (packet.mid > (message_buffer[index].offset + MAX_BUFFER_SIZE))  {
							enq(&overflow[index], packet);
							printdb("-enq ");
						}
						else  {
							printdb("-put ");
							overflow_size[index]--;
							sequence_num = (buffer_put (&message_buffer[index], packet.lts, packet.dat, packet.mid)) - 1;
							if (sequence_num >= 0)  {
								elm = buffer_get(&message_buffer[index], sequence_num);
								max_lts[index] = elm->lts;
							}
						}
						printdb("\n");

					}
				}
			
				 
				 
				for(index = 0; index < num_machines; index++) {
				  out_msg.payload[index*NAK_QUOTA] = 0;
				  if ((index == me) || (sending[index] == INACTIVE)) {
						continue;
				  }
					printsel(" / PID-%d [", index);
					j = 1;
					this_nak = buffer_first_open(&message_buffer[index]);
					printsel(" firstopen(%d) ", this_nak);
					if (this_nak == last_nak[index]) {
						last_nak_count[index]++;
					}
					else {
						last_nak[index] = this_nak;
						last_nak_count[index] = 0;
					}
					if ((last_nak_count[index] >= 10) || (last_message[index] > 0)) {
						last_nak_count[index] = 0;
						naklast++;
						nak_count++;
						out_msg.payload[index * NAK_QUOTA]++;
						out_msg.payload[(index * NAK_QUOTA) + j] = this_nak;
						printsel(" LASTNAK_Msg#%d, ", this_nak);
						j++;
					}
					printsel("this_nak(%d) ",this_nak);
					
					for (i=message_buffer[index].offset; i <= message_buffer[index].upperlimit; i++) {
						if (message_buffer[index].upperlimit - i < NAK_BACKOFF)  {
							break;
						}
						printsel("OTHERNAKs ");
						if (!buffer_isActive(&message_buffer[index], i)) {
							nak_count++;
							out_msg.payload[index * NAK_QUOTA]++;
							out_msg.payload[(index * NAK_QUOTA) + j] = i;
							printsel(" Msg#%d, ", i);
							j++;
							if (j == NAK_QUOTA)  {
								break;
							}
						}
					}
					
					printsel("]");
				}
				printsel("\n");
				print_buffers_select(message_buffer, num_machines);
				/*  SEND NAK MESSAGE  */
				if (nak_count > 0)  {
					printdb("SENDING NAK for %d lost messages!\n", nak_count); 
					numnak+=nak_count;
					naksent++;
					sendto(ss,(char *) &out_msg, sizeof(Message), 0,
					   (struct sockaddr *)&send_addr, sizeof(send_addr));
				}
				nak_count = 0;
			gettimeofday(&d2, NULL);
			naksenddiff+= time_diff(d1, d2);
			}

			/******************************************
			 *   SEND DATA Message
			 *****************************************/
			if (sending[me] != INACTIVE)  {
				out_msg.tag = DATA_MSG;
				
				cur_maxdel = get_min(max_delivered, num_machines);
				cur_maxack = msg_count+1;
				for (index = 0; index < num_machines; index++)  {
					if (index == me)  {
						continue;
					}
					if ((max_ack[index] > -1) && (max_ack[index] < cur_maxack))  {
						cur_maxack = max_ack[index];
					}
				}
				if ((cur_maxack - cur_maxdel) > MAX_BUFFER_SIZE) {
					send_count = msg_count + 1;
				}
				else {
					send_count = msg_count + BATCH_SEND;
				}
				max_send = cur_maxdel + MAX_BUFFER_SIZE; 
				
				printdb("Highest possible send:  %d   \n", send_count);
				while ( (msg_count < num_packets) &&
/*						(msg_count < send_count)  &&     */
						(msg_count < max_send)  &&
						(!buffer_isFull(&out_buffer)) &&
						(!buffer_isFull(&message_buffer[me]))) {
					lts++;
					printdb("Sending DATA MSG #%d   (LTS=%d)\n", msg_count, lts);

					 /* DATA MESSAGE:
						*  payload[0] = message ID (mid)
						*  payload[1] = Lamport TimeStamp (LTS)
						*  payload[2] = Data Value  */
					out_msg.payload[0] = msg_count++;
					out_msg.payload[1] = lts; 
					out_msg.payload[2] = (rand() % 999999) + 1;
				
					max_lts[me] = lts;
					max_ack[me]++;
					
					buffer_append(&message_buffer[me], out_msg.payload[1], out_msg.payload[2]);
					buffer_append(&out_buffer, out_msg.payload[1], out_msg.payload[2]);
					sendto(ss,(char *) &out_msg, sizeof(Message), 0,
						   (struct sockaddr *)&send_addr, sizeof(send_addr));
					have_sent++;
					if (msg_count == num_packets)  {
						last_message[me] = msg_count-1;
					}
					if (msg_count == display_count) {
						display_count += display_interval;
						printinfo("  Sent %7d Messages.\n", msg_count);
					}
					
				}

				/******************************************
				 *   RE-SEND Lost Data Message(s)
				 *****************************************/
				if (resend_count >= RESEND_TRIGGER)  {
					resend_count = 0;
					while(!isEmpty(lost_packets)) {
					  packet = deq(&lost_packets);
					  index = packet.mid;
					  printdb("NEED to RE-SEND %d: ", index);
					  /* Ensure packet is is my window  */
					  if ((index >= out_buffer.offset) && (index <= out_buffer.upperlimit) ) {
						printdb("Resending packet %d\n", index);
						out_msg.tag = DATA_MSG;
						elm = buffer_get(&out_buffer, index);
						out_msg.payload[0] = index;
						out_msg.payload[1] = elm->lts;
						out_msg.payload[2] = elm->data;
						printdb("Re-Sending DATA MSG #%d   (LTS=%d)\n", index, elm->lts);
						resend_addr.sin_addr.s_addr = host_list[packet.pid];

						/*  Re-send mutli-cast  */
/*						sendto(ss,(char *) &out_msg, sizeof(Message), 0,
							   (struct sockaddr *)&send_addr, sizeof(send_addr));
*/							
						/*  Re-send uni-cast  */
						sendto(su,(char *) &out_msg, sizeof(Message), 0,
							   (struct sockaddr *)&resend_addr, sizeof(resend_addr));
						resend++;
						}
					}
					printdb("\n");
				}
			}
		}

		/*==========================================================
		 * 	  (2) Listen to Socket & Receive Data (or timeout)
		 *==========================================================*/
		 { 
		/*  reset default timeout & mask vals  */
		temp_mask = mask;
		
		/*  IF IN IDLE STATE: set longer timeout & Listen */
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
		if (receive_count == 0)  {
			sock_num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &timeout);
			if ((sock_num > 0) && (FD_ISSET(sr, &temp_mask))) { 
				if (state != IDLE)  {
					waitrecvtime += (TIMEOUT_RECV - timeout.tv_usec);
				}
				receive_amount = (state == IDLE) ?
					recv(sr, mess_buf, sizeof(mess_buf), 0) :
					recvfrom_dbg(sr, mess_buf, sizeof(mess_buf), 0, &from_addr, &from_len);
				from_ip = from_addr.sin_addr.s_addr;
	/*				recv_dbg(sr, mess_buf, sizeof(mess_buf), 0);   */
	/*				recv_dbg(sr, one_mess, sizeof(one_mess), 0);   */

				if (receive_amount == 0) {
				  printdb("DROPPED A PACKET\n");
				  continue;
				}
/*				printinfo("# packets Received: %d\n", receive_amount);   */
				receive_count = 0;
			} else if (sock_num == 0) {
				receive_count = 0;
				if (state != IDLE)  {
					totime += (TIMEOUT_RECV - timeout.tv_usec);
				}
				to++;
				
				/*  On Timeout: Accelerate sending of NAK & STATUS Messages  */
				status_count += TRIGGER_ACCELERATOR;  
				nak_count    += TRIGGER_ACCELERATOR;   
				if (state == RECV)  {
					state = SEND;			
				}
				printdb("timed_out when trying to receive\n");
				continue;
			} else {
				printerr(" ERROR! SOCK NUM %d", sock_num);
				exit(0);
			}
		}

		in_msg = (Message *) &(mess_buf[receive_count]);
/*		printinfo("RECV AMT= %d, CNT=%d\n", receive_amount, receive_count);  */
		receive_count += MAX_PACKET_SIZE;
		from_pid = in_msg->pid;

		if ((from_ip != 0) && (host_list[from_pid] == 0)) {
			host_list[from_pid] = from_ip;
			printdb("NEW IP: pid=%d, ip=%d\n", from_pid, host_list[from_pid]);
		}
		
		/*  Ignore my messages  */
		if (from_pid == me)  {
			if (receive_count >= receive_amount) {
				receive_count = 0;
				state = SEND;
			}
			continue;
		}
		 }
		
		/*==========================================================
		 * (3)  RECV STATE Algorithm Implementation
		 *==========================================================*/
		switch (in_msg->tag)  {
			
			/****************************************************
			 *  PROCESS the Start Signal to get out of IDLE State
			 *****************************************************/
			case GO_MSG:
				printinfo("Received Start signal. Multicasting...\n");
				gettimeofday(&start_t, NULL);
				state = SEND;
				break;
		
			/****************************************************
			 *  PROCESS a DATA message
			 *****************************************************/
			case DATA_MSG:
				/*  STORE DATA  & UPDATE MAX_MID RECV STATE */
				printdb("Received DATA MSG from PID-%d, MID=%d, LTS=%d\n", from_pid, in_msg->payload[0], in_msg->payload[1]);
				if (in_msg->payload[1] > lts)  {
					lts = in_msg->payload[1];
				}
				/*  SET: highest in-order message ID from the last sender, "OPEN-1"   */
							 /* DATA MESSAGE:
								*  payload[0] = message ID (mid)
								*  payload[1] = Lamport TimeStamp (LTS)
								*  payload[2] = Data Value  */

					/*  CHECK: Duplicate packet  */
					if (buffer_isActive(&message_buffer[from_pid], in_msg->payload[0])) {
						printdb("  **Received DUPLICATE Packet.\n");
						dup++;
					}
					/*  CHECK: Old Packet */
					else if (in_msg->payload[0] < message_buffer[from_pid].offset) {
						printdb("RECEIVED AN OLD PACKET\n");
						old++;
					}
					/*  CHECK: INDEX OOB; place in OVERFLOW buffer */
					else if (in_msg->payload[0] >= (message_buffer[from_pid].offset + MAX_BUFFER_SIZE)) {
						packet.mid = in_msg->payload[0];
						packet.lts = in_msg->payload[1];
						packet.dat = in_msg->payload[2];
						enq(&overflow[from_pid], packet); 
						overflow_size[from_pid]++;
						printdb("******OVERFLOWING Message due to lack of buffer size on %d ******\n", from_pid);
						oob++;
					}
					else {
						sequence_num = (buffer_put (&message_buffer[from_pid], in_msg->payload[1], in_msg->payload[2], in_msg->payload[0])) - 1;
						printdb(" FIRST OPEN IS: %d\n", sequence_num);

						/*  SET: track LTS associated with that message ID */
						if (sequence_num >= 0)  {
							elm = buffer_get(&message_buffer[from_pid], sequence_num);
							max_lts[from_pid] = elm->lts;
						}
						print_buffers(message_buffer, num_machines);
/*						store++;
						if (store == store_count) {
							store_count += display_interval;
							printinfo("  Stored %7d Messages.\n", store);
						}
*/
					}
				
				break;
			
			
			/****************************************************
			 *  PROCESS a STATUS message
			 *****************************************************/
			case STATUS_MSG:
				gettimeofday(&d1, NULL);

				/*  UPDATE: Sending state for this process */
				sending[from_pid] = in_msg->payload[1+MAX_MACHINES+from_pid];
				printdb("Received STATUS MSG from PID %d, its status = %d\n", from_pid, sending[from_pid]);
				
				/*  UPDATE: Highest ACK this process has acknowledged for my outgoing message */
				/* TODO:  CHECK IF I AM NOT INACTIVE  */
				if (in_msg->payload[me] > max_ack[from_pid])  {
					max_ack[from_pid] = in_msg->payload[me];
				}
				
				if (in_msg->payload[1+(2*MAX_MACHINES)+me] > max_delivered[from_pid]) {
					max_delivered[from_pid] = in_msg->payload[1+(2*MAX_MACHINES)+me];
				}
				
				cur_minmid = get_min(max_ack, num_machines);

				/*  CHECK: Am I done sending Messages?  */
				printdb(" MAX ACK = ");
				printarray(max_ack, num_machines);
				if ((msg_count >= num_packets-1) && (cur_minmid >= num_packets-1))  {
					printstatuses(sending, num_machines);
					printdb("END OF MESSAGES  ------------- GOING");
					if ((sending[me] == ACTIVE) || (sending[me] == DONE_SENDING))  {
						printdb("...DONE_SENDING\n");
						sending[me] = DONE_SENDING;
					}
					else if ((sending[me] == DONE_DELIVERING) || (sending[me] == INACTIVE))  {
						printdb("...INACTIVE\n");
						sending[me] = INACTIVE;
					}
					else {
						printdb(" TO A BAD STATE");
						exit(0);
					}
					/*  CHANGE max_mid to max_ack  */
					max_ack[me] = -1;
				}
				
				/*  UPDATE: Last Message Count from this PID, if provided  */
				last_message[from_pid] = in_msg->payload[MAX_MACHINES];
				
				printstatuses(sending, num_machines);


				
				/* STATUS CHECK:  Has everyone completed sending messages  */
				all_inactive = TRUE;
				printdb("Check all inactive: ");
				for (index=0; index<num_machines; index++) {
					if (sending[index] != INACTIVE)  {
						printdb(" pid %d prevents all_inactive", index);
						all_inactive = FALSE;
					}
				}
				
				/*  IF no one is sending anything, shut down  */
				if (all_inactive)  {
					printdb("ALL INACTIVE -- GOING TO KILL STATE\n");
					state = KILL;
					break;
				}
				
				/* CAN I flush the out-going buffer?  */
				if (cur_minmid >= out_buffer.offset)  {
					buffer_clear (&out_buffer, (cur_minmid - out_buffer.offset + 1));
				}
				
				/****************************************************
				 *  DELIVERY ALGORITHM 
				 *****************************************************/
				/*  FIND the highest possible LTS for message delivery  */
				deliver_lts = 10000000;
				for (index = 0; index < num_machines; index++) {
					if ((max_lts[index] >= 0) && (max_lts[index] < deliver_lts))  {
						deliver_lts = max_lts[index];
					}
				}
				printdb("\n GO-Deliver to %d.  MAX LTS=", deliver_lts);
				printarray(max_lts, num_machines);
				printdb("\n Last Messages  =");
				printarray(last_message, num_machines);

				if ((deliver_lts >= 0) && (deliver_lts < 10000000))  {  
					while (TRUE)  {
						cur_minlts = 10000000;
						for (index=0; index <num_machines; index++)  {
							if (max_lts[index] < 0)  {
								printdb("pid %d max lts is -1\n", index);
								continue;
							}
							if (buffer_isEmpty(&message_buffer[index]))  {
								printdb("pid %d buffer is empty\n", index);
								continue;
							}
							if (!buffer_isStartActive(&message_buffer[index])) {
								printdb("pid %d buffer start inactive\n", index);
								continue;
							} 
							elm = buffer_get(&message_buffer[index], message_buffer[index].offset);
							if ((elm->lts < cur_minlts) && (elm->lts >= 0)) {
								cur_minlts = elm->lts;
								cur_minpid = index;
								printdb("WILL TRY TO DELIVER LTS: %d, PID: %d\n", cur_minlts, cur_minpid);
							}
						}
						printdb(" minlts=%d\tdel_lts=%d\n", cur_minlts, deliver_lts);
						if (cur_minlts <= deliver_lts)  {
							printdb("DELIVERING:  LTS=%d,PID=%d\n", cur_minlts, cur_minpid);
							if (cur_minpid == me)  {
								max_delivered[me] = message_buffer[me].offset;
							}
							/*  WRITE HERE  */
							elm = buffer_get(&message_buffer[cur_minpid], message_buffer[cur_minpid].offset);
							fprintf(sink, "%2d, %8d, %8d\n", cur_minpid+1, message_buffer[cur_minpid].offset, elm->data);
							buffer_clear(&message_buffer[cur_minpid], 1);
					}
						else  {
							break;
						}
					}
					
					/* CHECK: Have I delivered all messages for PIDs?  */
					for (index=0; index<MAX_MACHINES; index++) {
/*						printdb("CHECK last deliver:  lm=%d,   off=%d", last_message[index], message_buffer[index].offset); */
						if ((last_message[index] >= 0) && (message_buffer[index].offset > last_message[index])) {
							printdb(" ----  DELIVERED LAST PACKET FOR PID %d -- Resetting LTS to -1\n", index);
							max_lts[index] = -1;  
						}
					}
					
					/*  CHECK: Have I delivered all messages?? */
					printdb("CHECK ALL DELIVERED. Last messages: ");
					printarray(last_message, num_machines);
					all_delivered = TRUE;
					for (index=0; index<MAX_MACHINES; index++) {
						if (((sending[index] == ACTIVE) && (last_message[index] < 0)) ||
							((last_message[index] >= 0) && (message_buffer[index].offset < last_message[index])))  { 
							printdb(" Seems %d is not done delivering", index);
							all_delivered = FALSE;
							break;
						}
					}

					printdb("\n");
					/*  IF: I have delivered all message move out of Active State */
					if (all_delivered)  {
						printstatuses(sending, num_machines);
						printdb("ALL DELIVERED  -----  GOING ");
						if ((sending[me] == ACTIVE) || (sending[me] == DONE_DELIVERING))  {
							printdb("... DONE_DELIVERING\n");
							sending[me] = DONE_DELIVERING;
						}
						else if ((sending[me] == DONE_SENDING) || (sending[me] == INACTIVE))  {
							printdb("... INACTIVE\n");
							sending[me] = INACTIVE;
						}
						else {
							printdb(" to a BAD STATE");
							exit(0);
						}
					}


					/*  CHECK: Overflow buffer  */
					for (index=0; index<num_machines; index++)  {
						if ((index == me) || (overflow_size[index] == 0))  {
							continue;
						} 
						printdb("CHECK overflow on %d: ", index);
						for (i=0; i<overflow_size[index]; i++)  {
							packet = deq(&overflow[index]);
							printdb("%d", packet.mid);
							if (packet.mid > (message_buffer[index].offset + MAX_BUFFER_SIZE))  {
								enq(&overflow[index], packet);
								printdb("-enq ");
							}
							else  {
								printdb("-put ");
								overflow_size[index]--;
								sequence_num = (buffer_put (&message_buffer[index], packet.lts, packet.dat, packet.mid)) - 1;
								if (sequence_num >= 0)  {
									elm = buffer_get(&message_buffer[index], sequence_num);
									max_lts[index] = elm->lts;
								}
							}
/*							if (isEmpty(overflow[index])) {
								printdb("\n");
								break;
							}
*/							printdb("\n");

						}
					}
				}
				/*===========  END of DELIVERY ALGORITYM  ================*/
				
				printstatuses(sending, num_machines);
				print_buffers(message_buffer, num_machines);
				gettimeofday(&d2, NULL);
				ddiff += time_diff(d1, d2);
				break;
				
			
			/****************************************************
			 *  PROCESS a NAK message
			 *****************************************************/
			case NAK_MSG:
				/*  Update LOST msg queue  */
				gettimeofday(&d1, NULL);
				printdb("------Received NAK from %d for: ", from_pid);
				my_nakpos = me*NAK_QUOTA;
				num_naks = in_msg->payload[me*NAK_QUOTA];
				for (i=1; i <= num_naks; i++ ) {
					index = in_msg->payload[my_nakpos+i];
					printdb(" Msg#%d", index);
					if ((index < out_buffer.offset) || (index >= out_buffer.upperlimit)) {
						printdb("<Ignore%d >= %d>", index, out_buffer.upperlimit);
					}
					else {
						printdb("<Enq>");
						packet.pid = from_pid;
						packet.mid = index;
						enq(&lost_packets, packet);
					}
				}

				printdb("\n");
				gettimeofday(&d2, NULL);
				naksenddiff+= time_diff(d1, d2);
				break;
			
			/****************************************************
			 *  RECEIVE a KILL message
			 *****************************************************/
			case KILL_MSG:
				printdb("RECEIVED KILL CODE from PID %d\n", from_pid);
				printstatuses(sending, num_machines);
				state = KILL;
				break;
				
			default:
				printdb("Process Received erroneous message");
		}

		if (state == KILL)  {
			break;
		}
		if (receive_count >= receive_amount) {
			receive_count = 0;
			state = SEND;
		}

	} 
	fclose(sink);
	
	if (state != KILL)  {
		printerr("Program ended prematurely\n");
		printerr(" %-20s%6d\n", "MSG Count:",msg_count);
		printerr(" %-20s%6d\n", "Status:",sending[me]);		
		printinfo("%-20s%6d\n", " Duplicate Packets:", dup);
		printinfo("%-20s%6d\n", " Old Packets:", old);
		printinfo("%-20s%6d\n", " Index OOB:", oob);
		printinfo("%-20s%6d\n", " Resent Packets:", resend);
		printinfo("%-20s%6d\n", " Last NAK Case:", naklast);
		printinfo("%-20s%6d\n", " NAKs Triggered:", naktrig);
		printinfo("%-20s%6d\n", " NAK  Msgs Sent:", naksent);
		printinfo("%-20s%6d\n", " NAKs Requested:", numnak);
		printinfo("%-20s%6d\n", " # Timeouts:", to);
		printinfo("%-20s%6ld msec\n", " Time Wait & Timeout:", totime/1000);
		printinfo("%-20s%6ld msec\n", " Time Wait & Receiving:", waitrecvtime/1000);
		return(1);
	}
	else {
		out_msg.tag = KILL_MSG;
		printinfo("All Messages complete: %d messages.\nTerminating...\n\n", msg_count);
		gettimeofday(&end_t, NULL);
		printinfo("  Settings: \t MAX_BUFFER_SIZE= \t%d\n\t\t BATCH_SEND= \t\t%d\n\t\t STATUS_TRIGGER= \t%d\n\t\t NAK_TRIGGER= \t\t%d\n\t\t NAK_BACKOFF= \t\t%d\n\n", MAX_BUFFER_SIZE, BATCH_SEND, STATUS_TRIGGER, NAK_TRIGGER, NAK_BACKOFF);
		printinfo("%-25s%8d\n", " Duplicate Packets:", dup);
		printinfo("%-25s%8d\n", " Old Packets:", old);
		printinfo("%-25s%8d\n", " Index OOB:", oob);
		printinfo("%-25s%8d\n", " Full Buffer:", full);
		printinfo("%-25s%8d\n", " Resent Packets:", resend);
		printinfo("%-25s%8d\n", " Last NAK Case:", naklast);
		printinfo("%-25s%8d\n", " NAKs Triggered:", naktrig);
		printinfo("%-25s%8d\n", " NAKs Msgs Sent:", naksent);
		printinfo("%-25s%8d\n", " NAKs Requested:", numnak);
		printinfo("%-25s%8d (%2.1f%%)\n", " More Full Buffers:", morefull, (((float)morefull*100)/(morefull+lessfull)));
		printinfo("%-25s%8d (%2.1f%%)\n", " Less Full Buffers:", lessfull, (((float)lessfull*100)/(morefull+lessfull)));
		printinfo("%-25s%8d\n", " # Timeouts:", to);
		printinfo("%-25s%8ld msec\n", " Time Wait & Timeout:", totime/1000);
		printinfo("%-25s%8ld msec\n", " Time Wait & Receiving:", waitrecvtime/1000);
		printinfo("Status Msg Proc Time:   \t%2.3f secs\n", ddiff); 
		printinfo("NAK-Send Proc Time:     \t%2.3f secs\n", naksenddiff); 
		printinfo("NAK-Recv Proc Time:     \t%2.3f secs\n\n", nakrecvdiff); 
		printinfo("TOTAL TIME:	\t%2.2f secs\n\n", time_diff(start_t, end_t)); /*.tv_usec - start_t.tv_usec)/10000);*/

	/*  RE-SEND KILL Message N-Times  */
		for (index = 0; index < num_machines; index++)  {
			printdb("SEND: KILL Code #%d\n", index);
			sendto(ss,(char *) &out_msg, sizeof(Message), 0,
			   (struct sockaddr *)&send_addr, sizeof(send_addr));
		}
		printdb("\n Terminating.\n-------------------------------\n\n");
	}
	return 0;
}


void printarray(int *v, int n)  {
	int i;
	for (i=0; i<n; i++)  {
		printdb("  %3d,", v[i]);
	}
	printdb("\n");
}

void printstatuses (int * v, int n)  {
	int i;
	printdb("STATUSES: ");
	for (i=0; i<n; i++)  {
		printdb("  %2d,", v[i]);
	}
	printdb("\n");
	
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

void print_buffers_select(buffer *b, int num_machines) {
  int i;
  for(i=0; i < num_machines; i++) {
  printsel("-----------------------\n");
    printsel("P%d:\n", i+1);
    buffer_print_select(&b[i]);
  printsel("-----------------------\n");
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

/*  OTHER HELPER functions */
float time_diff (struct timeval s, struct timeval e)
{
	float usec;
	usec = (e.tv_usec > s.tv_usec) ? 
		(float)(e.tv_usec-s.tv_usec) :
		(float)(s.tv_usec-e.tv_usec)*-1;
	return ((float)(e.tv_sec-s.tv_sec) + usec/1000000);
	
}