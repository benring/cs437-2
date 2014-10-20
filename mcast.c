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
	int enable_timeout = TRUE;
	int sum = 0;
	int count = 0;
	float avg = 0.0;

	/* Networking vars */
	struct sockaddr_in    recv_addr;
	struct sockaddr_in    send_addr;
	struct sockaddr_in    from_addr;
	struct sockaddr_in    resend_addr;
	socklen_t             from_len;
	
	struct ip_mreq			mreq;
	unsigned char			ttl_val;
	int                   ss,sr, su;
	fd_set                mask;
	fd_set                write_mask;
	fd_set                dummy_mask,temp_mask, write_temp_mask;
	int                   bytes;
	int                 	sock_num;
	char                	mess_buf[MAX_PACKET_SIZE];
	struct timeval  		timeout, write_timeout;

	/* Message structs for interpreting bytes */
	Message         out_msg;
	Message			*in_msg;
	Value			*elm;

	/* Process's Internal State */
	int				state;
	int				me;
	int				num_machines;
	int				lts;
	int				status_count;
	int				nak_count;
	int 			resend_count;
	int				receive_count;
	int				send_count;
	int 			max_send;
	int				loss_rate;
	int				all_inactive;
	int				all_delivered;
	int				prev_sender;
	int				prev_sender_count;
	int				batch_num;
	int 			my_turn;
	int				host_list[MAX_MACHINES];


	/*  Outgoing Message State (send)  */
	buffer			out_buffer;
	linked_list *   lost_packets[MAX_MACHINES];
	linked_list *   lost_packet_list;
	
	int				msg_count;
	int				num_packets;
	int				max_ack[MAX_MACHINES];
	int                             max_delivered[MAX_MACHINES];
	int				last_nak[MAX_MACHINES];
	int				last_nak_count[MAX_MACHINES];
	int				this_nak;

	/*  All Message State (receive)  */
	buffer			message_buffer[MAX_MACHINES];
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
	int				sequence_num;
	int				display_count;
	int				display_savedmsg_count;
	int 			display_interval;
	struct timeval  start_t;
	struct timeval  end_t;

	/*   Optimization vars  */
	int				dup;
	int				old;
	int				resend;
	int				naklast;
	int				naktrig;
	int				oob;
	int				full;
	int				to;
	long			totime;
	long			waitrecvtime;
	int				numnak;
	int                             numstatus;
	int				lessfull;
	int				morefull;
	int				saved_msg;
	int				sendmany;
	struct timeval  d1;
	struct timeval  d2;
	float			ddiff = 0.0;
	float			naksenddiff = 0.0;
	float			nakrecvdiff = 0.0;
	float                   trydeliverdiff = 0.0;
	float                   appenddatadiff = 0.0;
	float                   sendcalldatadiff = 0.0;



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


	/* Declare a variable to store the option */
	u_char foo;

	/*
	 *  * After you have set up your sending socket, and called setsockopt to set the ttl,
	 *   * you can set the loop option.
	 *    * Set loop to 0 if you would like to ignore your own packets.
	 *     */

	foo = 0;
	if (setsockopt(ss, IPPROTO_IP, IP_MULTICAST_LOOP, &foo, sizeof(foo)) < 0) {
		printf("Mcast: problem in setsockopt of multicast loop\n");
	}


	send_addr.sin_family = AF_INET;
	send_addr.sin_addr.s_addr = htonl(MCAST_ADDR);
	send_addr.sin_port = htons(PORT);

	/*  Set up Unicast Socket for re-sending data packets */
	su = socket(AF_INET, SOCK_DGRAM, 0); /* socket for sending (udp) */
	if (su<0) {
		perror("Ucast: socket");
		exit(1);
	}

	resend_addr.sin_family = AF_INET;
	resend_addr.sin_port = htons(PORT);
	from_len = sizeof(resend_addr);

	FD_ZERO( &mask );
	FD_ZERO( &dummy_mask );
	FD_ZERO( &write_mask);
	FD_SET( sr, &mask );
	FD_SET( ss, &write_mask);

	/*  Initialize Program State Variables  */
	state = IDLE;
	lts = -1;
	deliver_lts = -1;
	have_sent = -1;
	msg_count = 0;
	receive_count = 0;
	status_count = 0;
	nak_count = 0;
	resend_count = 0;
	out_msg.pid = me;
	out_buffer = buffer_init();
	if (num_packets > 0)  {
		display_interval = num_packets / 10;
	}
	else {
		display_interval = num_machines * 10000;
	}
	display_count = display_interval;
	display_savedmsg_count = display_interval*num_machines;
	cur_minpid = 0;
	prev_sender = (me == 0) ? (num_machines-1) : me-1;
	prev_sender_count = 0;
	batch_num = 0;
	my_turn = FALSE;

	for (index=0; index < MAX_MACHINES; index++) {
		max_mid[index] = -1;
		max_lts[index] = -1;
		max_ack[index] = -1;
		max_delivered[index] = -1;
		last_nak[index] = 0;
		last_nak_count[index] = 0;
		message_buffer[index] = buffer_init();
		sending[index] = UNKNOWN;
		last_message[index] = -1;
		host_list[index] = 0;
	}

	/*  Initialize Optimization Variables  */
	dup=0;
	resend=0;
	oob=0;
	naklast=0;
	naktrig=0;
	to=0;
	totime=0;
	waitrecvtime=0;
	full=0;
	old=0;
	numnak=0;
	numstatus=0;
	morefull=0;
	lessfull=0;
	saved_msg=0;
	sendmany=0;

	/*  STATE CHANGE:  UNKNOWN to ACTIVE or INACTIVE */
	if (num_packets > 0)  {
		sending[me] = ACTIVE;
		max_mid[me] = 0;
	} else  {
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


	for(loop=0; loop<9000000; loop++) {
		/*==========================================================
		 * (1)  SEND STATE Algorithm Implementation
		 *==========================================================*/

		/******************************************
		 *   SEND STATUS Message
		 *****************************************/
		if (status_count >= STATUS_TRIGGER)  {
			status_count = 0;
			out_msg.tag = STATUS_MSG;
			for (index=0; index<MAX_MACHINES; index++) {
				/* max delievered */
				out_msg.payload[index] = max_mid[index];
				/* sending status */
				out_msg.payload[1+index+MAX_MACHINES] = sending[index];
				/* max received */
				out_msg.payload[1+index+(2*MAX_MACHINES)] = buffer_first_open(&message_buffer[index]) - 1;
			}
			if (have_sent >= num_packets-1)  {
				printdb("I've DELIVERED my last packet. # Packet status is now %d\n", have_sent);
				out_msg.payload[MAX_MACHINES] = have_sent;
			} else {
				out_msg.payload[MAX_MACHINES] = -1;
			}
			printdb("SENDING STATUS Msg");
			sendto(ss,(char *) &out_msg, sizeof(Message), 0,
			       (struct sockaddr *)&send_addr, sizeof(send_addr));

		}
		
			/******************************************
			 *   SEND NAK Message
			 *****************************************/
		if (nak_count >= NAK_TRIGGER)  {
				naktrig++;
				printsel("NAK TRIGGERED!  BUILD NAK: ");
				nak_count = 0;
				out_msg.tag = NAK_MSG;

				/*---------------------------------------
				 *  NAK HEURISTIC ALGORITHM
				 *---------------------------------------*/
				for(index = 0; index < num_machines; index++) {
					out_msg.payload[index*NAK_QUOTA] = 0;
					if ((index == me) || (sending[index] == INACTIVE)) {
						continue;
					}
					j = 1;
					this_nak = buffer_first_open(&message_buffer[index]);
					
					/* CHECK: Last NAK case; necessary if last packet in message is lost */
					if (this_nak == last_nak[index]) {
						last_nak_count[index]++;
					} else {
						last_nak[index] = this_nak;
						last_nak_count[index] = 0;
					}
					if ((last_nak_count[index] >= 10)|| 
						((last_nak_count[index] > 1) && (last_message[index] > 0))) {
						last_nak_count[index] = 0;
						naklast++;
						nak_count++;
						out_msg.payload[index * NAK_QUOTA]++;
						out_msg.payload[(index * NAK_QUOTA) + j] = this_nak;
						j++;
					}

					/* CHECK: Find missing packet from beginning of the buffer */
					for (i=message_buffer[index].offset; i <= message_buffer[index].upperlimit; i++) {
						if (message_buffer[index].upperlimit - i < NAK_BACKOFF)  {
							break;
						}
						if (!buffer_isActive(&message_buffer[index], i)) {
							nak_count++;
							out_msg.payload[index * NAK_QUOTA]++;
							out_msg.payload[(index * NAK_QUOTA) + j] = i;
							j++;
							if (j == NAK_QUOTA)  {
								break;
							}
						}
					}

					printsel("]");
				}
				printsel("\n");

				/*  SEND NAK MESSAGE  */
				if (nak_count > 0)  {
					printdb("SENDING NAK for %d lost messages!\n", nak_count);
					numnak+=nak_count;
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
		if ((state == SEND))  {
			resend_count++;

			int sent_data = FALSE;
			if (sending[me] != INACTIVE)  {

				out_msg.tag = DATA_MSG;
				if ((my_turn) || (prev_sender == me))  {
					send_count = msg_count + MAX_SEND_SIZE;
					sendmany++;
				}
				else {
					send_count = msg_count + MIN_SEND_SIZE;
				}
					  	
/*				send_count = msg_count + 1; */
/*				cur_minmid = get_min(max_delivered, num_machines);
				max_send = cur_minmid + MAX_BUFFER_SIZE + MAX_SEND_SIZE; 
*/				cur_minmid = get_min(max_ack, num_machines);
				max_send = cur_minmid + MAX_BUFFER_SIZE; 
				printdb("Highest possible send:  %d   \n", (max_send));

				/*  reset default timeout & mask vals  */
/*				write_temp_mask = write_mask;
				write_timeout.tv_sec = 0;
				write_timeout.tv_usec = 0;
*/			/*(select(FD_SETSIZE, &dummy_mask, &write_temp_mask, &dummy_mask, &write_timeout)) && */

				while ( (msg_count < num_packets) &&
				        (msg_count < max_send) &&
				        (msg_count < send_count)  &&  
				        (buffer_size(&out_buffer) < WINDOW_SIZE)
				) 
							
				{
/*					write_temp_mask = write_mask;
					write_timeout.tv_sec = 0;
					write_timeout.tv_usec = 0;
*/					lts++;
					printdb("Sending DATA MSG #%d   (LTS=%d)\n", msg_count, lts);

					/* DATA MESSAGE:
					*  payload[0] = message ID (mid)
					*  payload[1] = Lamport TimeStamp (LTS)
					*  payload[2] = Data Value  */
					out_msg.payload[0] = msg_count++;
					out_msg.payload[1] = lts;
					out_msg.payload[2] = (rand() % 999999) + 1; 

					max_lts[me] = lts;
					buffer_append(&message_buffer[me], out_msg.payload[1], out_msg.payload[2]);
					buffer_append(&out_buffer, out_msg.payload[1], out_msg.payload[2]);
					sendto(ss,(char *) &out_msg, sizeof(Message), 0,
					       (struct sockaddr *)&send_addr, sizeof(send_addr));
					have_sent++;
					sent_data = TRUE;
					sum++;
					max_ack[me]++;
					if (msg_count == display_count) {
						display_count += display_interval;
						printinfo("  Sent:       %7d Messages.\n", msg_count);
					}
				}
				my_turn = FALSE;

				/******************************************
				 *   RE-SEND Lost Data Message(s)
				 *****************************************/
				if (resend_count >= RESEND_TRIGGER)  {
					resend_count = 0;
					for (from_pid = 0; from_pid < num_machines; from_pid++) {
						if (from_pid == me) {
							continue;
						}
						resend_addr.sin_addr.s_addr = host_list[from_pid];
						/*  Uni-Cast */
						while(!isEmpty(lost_packets[from_pid])) {
							index = deq(&lost_packets[from_pid]);
						
						/* Multi-Cast
						while(!isEmpty(lost_packet_list)) {
							index = deq(&lost_packet_list);
						*/
							printdb("NEED to RE-SEND %d: ", index);
							/* Ensure packet is is my window  */

							if ((index > max_ack[from_pid]) && (index >= out_buffer.offset) && (index <= out_buffer.upperlimit) ) { 
/*							cur_minmid = get_min(max_ack, num_machines); 
							if ((index > cur_minmid) && (index >= out_buffer.offset) && (index <= out_buffer.upperlimit) ) {
*/								printdb("Resending packet %d\n", index);
								out_msg.tag = DATA_MSG;
								elm = buffer_get(&out_buffer, index);
								out_msg.payload[0] = index;
								out_msg.payload[1] = elm->lts;
								out_msg.payload[2] = elm->data;
								printdb("Re-Sending DATA MSG #%d   (LTS=%d)\n", index, elm->lts);
								
								/*  RE-Send via Multi-Cast  
								sendto(ss,(char *) &out_msg, sizeof(Message), 0,
								   (struct sockaddr *)&send_addr, sizeof(send_addr));
								*/

								/* RR-Send via Uni-Cast */
								sendto(su,(char *) &out_msg, sizeof(Message), 0,
								   (struct sockaddr *)&resend_addr, sizeof(resend_addr));
	
								resend++;
							}
						}
					}

					printdb("\n");
				}
				if (sent_data) {
					count++;
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
			timeout.tv_usec  = enable_timeout ? TIMEOUT_RECV : 0;
		}

		/*  Receive Data  */
		sock_num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &timeout);
		if ((sock_num > 0) && (FD_ISSET(sr, &temp_mask))) {
			bytes = (state == IDLE) ?
			        recv(sr, mess_buf, sizeof(mess_buf), 0) :
/*			        recv_dbg(sr, mess_buf, sizeof(mess_buf), 0);  */
					recvfrom_dbg(sr, mess_buf, sizeof(mess_buf), 0, &from_addr, &from_len);
			if ((state != IDLE) && enable_timeout)  {
				waitrecvtime += (TIMEOUT_RECV - timeout.tv_usec);
			}
			if (bytes == 0) {
				printdb("DROPPED A PACKET\n");
				continue;
			}
			in_msg = (Message *) mess_buf;
			receive_count++;

			enable_timeout = FALSE; /* try to recv again without blocking */
		} else if (sock_num == 0) {
			if (enable_timeout) {

				if (state != IDLE)  {
					totime += (TIMEOUT_RECV - timeout.tv_usec);
				}

				to++;
				/*  On Timeout: Accelerate sending of NAK & STATUS Messages   */
				status_count += (STATUS_TRIGGER / TRIGGER_DIVISOR); 
				nak_count    += (NAK_TRIGGER / TRIGGER_DIVISOR);
				
				/* FLOW_CONTROL: */
				prev_sender_count = 0;

			} else {
				enable_timeout = TRUE;

			}

			receive_count = BATCH_RECEIVE;
			if (state == RECV)  {
				state = SEND;
			}
			printdb("timed_out when trying to receive\n");
			continue;
		} else {
			printf(" SOCK NUM %d", sock_num);
			exit(0);
		}

		from_pid = in_msg->pid;
		if (host_list[from_pid] == 0) {
			host_list[from_pid] = from_addr.sin_addr.s_addr;
			printdb("NEW IP: pid=%d, ip=%d\n", from_pid, host_list[from_pid]);
		}

		/*  Ignore my messages  */
		if (from_pid == me)  {
			printerr("ERROR. Received my own msg.\n");
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
		case GO_MSG: {
			printinfo("Received Start signal. Multicasting...\n");
			gettimeofday(&start_t, NULL);
			
			/* FLOW_CONTROL: */
			my_turn = (me == 0) ? TRUE : FALSE;
			state = SEND;
			break;
		}
			/****************************************************
			 *  PROCESS a DATA message
			 *****************************************************/
		case DATA_MSG:

			status_count++;
			nak_count++;

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
			/*  CHECK: INDEX OOB  */
			else if (in_msg->payload[0] > (message_buffer[from_pid].offset + MAX_BUFFER_SIZE)) {
				printdb("******Ignoring Message due to lack of buffer size on %d ******\n", from_pid);
				oob++;
			} else {
				sequence_num = (buffer_put (&message_buffer[from_pid], in_msg->payload[1], in_msg->payload[2], in_msg->payload[0])) - 1;

				/*  SET: track LTS associated with that message ID */
				if (sequence_num >= 0)  {
					elm = buffer_get(&message_buffer[from_pid], sequence_num);
					max_lts[from_pid] = elm->lts;
				}
			}
			/*  FLOW_CONTROL:  */
			if (from_pid == prev_sender)  {
				prev_sender_count++;
				if (prev_sender_count > WINDOW_SIZE/8) {
					prev_sender_count = 0;
					my_turn = TRUE;
				}
			}
			
			break;


			/****************************************************
			 *  PROCESS a STATUS message
			 *****************************************************/
		case STATUS_MSG:
			numstatus++;

			/*  UPDATE: Sending state for this process */
			sending[from_pid] = in_msg->payload[1+MAX_MACHINES+from_pid];
			printdb("Received STATUS MSG from PID %d, its status = %d\n", from_pid, sending[from_pid]);
			
			/* FLOW_CONTROL:  update prev_sender if my prev_sender is INACTIVE */
			if ((from_pid == prev_sender) && (sending[from_pid] == INACTIVE)) {
				do {
					prev_sender--;
					if (prev_sender < 0) {
						prev_sender = num_machines - 1;
					}
				} while ((prev_sender != me) && (sending[prev_sender] == INACTIVE));
				printdb (" My Prev Sender = %d", prev_sender);
			}

			/*  UPDATE: Highest ACK this process has acknowledged for my outgoing message */
			/* TODO:  CHECK IF I AM NOT INACTIVE  */
			if (in_msg->payload[me] > max_delivered[from_pid])  {
				max_delivered[from_pid] = in_msg->payload[me];
			}
			if (in_msg->payload[1+(2*MAX_MACHINES)+me] > max_ack[from_pid])  {
				max_ack[from_pid] = in_msg->payload[1+(2*MAX_MACHINES)+me];
			}

			cur_minmid = get_min(max_delivered, num_machines);

			/*  CHECK: Am I done sending Messages?  */
			printdb(" MAX DELIVERED = ");
			if ((msg_count >= num_packets-1) && (cur_minmid >= num_packets-1))  {
				printdb("END OF MESSAGES  ------------- GOING");
				if ((sending[me] == ACTIVE) || (sending[me] == DONE_SENDING))  {
					printdb("...DONE_SENDING\n");
					sending[me] = DONE_SENDING;
				} else if ((sending[me] == DONE_DELIVERING) || (sending[me] == INACTIVE))  {
					printdb("...INACTIVE\n");
					sending[me] = INACTIVE;
				} else {
					printdb(" TO A BAD STATE");
					exit(0);
				}
				max_mid[me] = -1;
			}

			/*  UPDATE: Last Message Count from this PID, if provided  */
			last_message[from_pid] = in_msg->payload[MAX_MACHINES];

			/* CHECK:  Do I have all the packets this pid sends? */
			if ((last_message[from_pid] >= 0) && (message_buffer[from_pid].offset > last_message[from_pid])) {
				printdb("Have received ALL packets from pid %d thru %d", from_pid, in_msg->payload[MAX_MACHINES]);
				max_lts[from_pid] = -1;
			}


			/*  CHECK: Have I delivered all messages?? */
			printdb("CHECK ALL DELIVERED. Last messages: ");
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
				printdb("ALL DELIVERED  -----  GOING ");
				if ((sending[me] == ACTIVE) || (sending[me] == DONE_DELIVERING))  {
					printdb("... DONE_DELIVERING\n");
					sending[me] = DONE_DELIVERING;
				} else if ((sending[me] == DONE_SENDING) || (sending[me] == INACTIVE))  {
					printdb("... INACTIVE\n");
					sending[me] = INACTIVE;
				} else {
					printdb(" to a BAD STATE");
					exit(0);
				}
			}

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

			int cur_maxack;
			cur_maxack = get_min(max_ack, num_machines);
			printdb("MAX ACK: %d", cur_maxack);
			/* CAN I flush the out-going buffer?  */
			if (cur_maxack >= out_buffer.offset)  {
				buffer_clear (&out_buffer, (cur_minmid - out_buffer.offset + 1));
			}
			break;

			/****************************************************
			 *  PROCESS a NAK message
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
				if (j >= out_buffer.upperlimit) {
					printdb("<Ignore%d >= %d>", j, out_buffer.upperlimit);
				} else {
					printdb("<Enq>");
					/* Uni-Cast */
					if (search(lost_packets[from_pid], j) == FALSE) {
						enq(&lost_packets[from_pid], j);
					}
					
					/*Multi-Cast 
					if (search(lost_packet_list, j) == FALSE) {
						enq(&lost_packet_list, j);
					}
					*/
				}
			}
			printdb("\n");
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
		if (receive_count >= BATCH_RECEIVE) {
			state = SEND;
			receive_count = 0;
		}

		/****************************************************
		 *  DELIVERY ALGORITHM
		 *****************************************************/

		/*  CHECK LTS for message delivery  */
		printdb(" MAX LTS=");

		deliver_lts = 10000000;
		printstatuses(sending, num_machines);
		for (index = 0; index < num_machines; index++)  {
			if (sending[index] == UNKNOWN)  {
				printdb("UNKNOWN STATUS FROM pid-%d\n", index);
				deliver_lts = -1;
				break;
			}
			if ((sending[index] == ACTIVE) && (max_lts[index] < 0))  {
				printdb("DETECTED ACTIVE STATE, but have not received any data yet from pid %d\n", index);
				deliver_lts = -1;
				break;
			}
			if ((max_lts[index] < deliver_lts) && (max_lts[index] >= 0))  {
				deliver_lts = max_lts[index];
				printdb("Check DELIVER thru LTS: %d (this active pid=%d)\n", deliver_lts, index);
			}
		}

		if ((deliver_lts >= 0) && (deliver_lts < 10000000))  {
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
					printdb("DELIVERING:  LTS=%d,PID=%d\n", cur_minlts, cur_minpid);
					/*  WRITE HERE  */
					elm = buffer_get(&message_buffer[cur_minpid], message_buffer[cur_minpid].offset);
					fprintf(sink, "%2d, %8d, %8d\n", cur_minpid+1, message_buffer[cur_minpid].offset, elm->data);
					saved_msg++;
/*					if (saved_msg == display_savedmsg_count) {
						display_savedmsg_count += display_interval * (num_machines);
						printinfo("  Delivered   %7d Messages.\n", saved_msg);
					}
*/

					/*  Set MAX_MID to what you are about to deliver  */
					max_mid[cur_minpid] = message_buffer[cur_minpid].offset;
					buffer_clear(&message_buffer[cur_minpid], 1);

					/*  Special cases for delivering your own packets  */
					if (cur_minpid == me) {
						/*  If you have delivered your last message, reset your max-mid to -1  */
						max_delivered[me]++;
						if (max_mid[me] >= num_packets-1) {
							printdb(" ----  DELIVERED MY OWN LAST PACKET -- Resetting LTS to -1\n");
							/*									sending[me] = DONE_DELIVERING;  */
							max_lts[me] = -1;
						} else {
							max_mid[me] = message_buffer[me].offset;
						}
					}
				} else  {
					break;
				}
			}
		}
		/*===========  END of DELIVERY ALGORITYM  ================*/

	}
	fclose(sink);

	if (state != KILL)  {
		printdb("Program ended prematurely\n");
		return(1);
	} else {
		avg = (1.0 * sum) / count;
		out_msg.tag = KILL_MSG;
		printinfo("All Messages complete: %d messages.\nTerminating...\n\n", msg_count);
		gettimeofday(&end_t, NULL);
		printinfo("  Settings: \t MAX_BUFFER_SIZE= \t\t%d\n\t\t MAX_SEND_SIZE= \t\t%d\n\t\t STATUS_TRIGGER= \t%d\n\t\t NAK_TRIGGER= \t\t%d\n\t\t NAK_BACKOFF= \t\t%d\n\n", MAX_BUFFER_SIZE, MAX_SEND_SIZE, STATUS_TRIGGER, NAK_TRIGGER, NAK_BACKOFF);
		printinfo("%-25s%9f\n", " Avg Packets per send", avg);
		printinfo("%-25s%8d\n", " Duplicate Packets:", dup);
		printinfo("%-25s%8d\n", " Old Packets:", old);
		printinfo("%-25s%8d\n", " Index OOB:", oob);
		printinfo("%-25s%8d\n", " Full Buffer:", full);
		printinfo("%-25s%8d\n", " Resent Packets:", resend);
		printinfo("%-25s%8d\n", " Max Send Count:", sendmany);
		printinfo("%-25s%8d\n", " Last NAK Case:", naklast);
		printinfo("%-25s%8d\n", " NAKs Triggered:", naktrig);
		printinfo("%-25s%8d\n", " NAKs Requested:", numnak);
		printinfo("%-25s%8d\n", " STATUS Received:", numstatus);
		printinfo("%-25s%8d\n", " # Timeouts:", to);
		printinfo("%-25s%8ld msec\n", " Time Wait & Timeout:", totime/1000);
		printinfo("%-25s%8ld msec\n", " Time Wait & Receiving:", waitrecvtime/1000);
		printinfo("TOTAL TIME:   %2.2f secs\n\n", time_diff(start_t, end_t)); /*.tv_usec - start_t.tv_usec)/10000);*/

		/*  RE-SEND KILL Message N-Times  */
		for (index = 0; index < num_machines; index++)  {
			printdb("SEND: KILL Code #%d\n", index);
			sendto(ss,(char *) &out_msg, sizeof(Message), 0,
			       (struct sockaddr *)&send_addr, sizeof(send_addr));
		}
		printdb("\n Terminating-------------------------------\n\n");
	}
	return 0;
}


void printarray(int *v, int n)
{
	int i;
	for (i=0; i<n; i++)  {
		printdb("  %3d,", v[i]);
	}
	printdb("\n");
}

void printstatuses (int * v, int n)
{
	int i;
	printdb("STATUSES: ");
	for (i=0; i<n; i++)  {
		printdb("  %2d,", v[i]);
	}
	printdb("\n");

}

void print_line()
{
	printdb("-----------------------\n");

}
void print_buffers(buffer *b, int num_machines)
{
	int i;
	for(i=0; i < num_machines; i++) {
		print_line();
		printdb("P%d:\n", i);
		buffer_print(&b[i]);
		print_line();
	}

}

void print_buffers_select(buffer *b, int num_machines)
{
	int i;
	for(i=0; i < num_machines; i++) {
		printsel("-----------------------\n");
		printsel("P%d:\n", i+1);
		buffer_print_select(&b[i]);
		printsel("-----------------------\n");
	}

}


int get_min(int * a, int num)
{
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
