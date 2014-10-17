/******************************************************************************
 *  File:  buffer.c
 *
 *  Author: Benjamin Ring & Josh Wheeler
 *  Date:   15 October 2014
 *
 *  Description:  The circular buffer array uses a static array with a start, end
 *   & open value which reference internal indices in the array. For external
 * 	 referencing, the values offset & upperlimit should be used. Offset is the
 *   position of the "start" element in the buffer and upperlimit refers to the
 *   highest index-able value in the buffer at any given time. The values of 
 *   start, end, and open are always in the range [0..MAX_BUFFER_SIZE]. Offset and
 *   upperlimit are positive ints. Upperlimite is always greater than or equal to
 * 	 offset, but never greater than offset + MAX_BUFFER_SIZE. 
 * 	 Elements are only removed from the start.
 *   Elements can be abitrarily inserted into any position between offset..MAX_BUFFER_SIZE
 *   Elements can also be appended to the end.
 *   Implementation provides arbitrary indexing of elements relative to zero. In this way, 
 *   the buffer acts as a "sliding window" within a larger data set of Values. 
 *   In addition, the buffer will always contain one "dummy" slot which is never
 *   filled, thus the actual memory used will be MAX_BUFFER_SIZE + 1.
 *******************************************************************************/
#ifndef BUFFER_H
#define BUFFER_H
#include "config.h"

typedef struct buffer {
   int start;			/* Internal "start" position of window buffer*/
   int end;				/* Internal "end" position of window buffer */
   int open;			/* Internal reference of the first open slot in window */
   int offset;			/* External reference index of the first slot in window */
   int upperlimit;		/* External reference index of the last slot in window */
   int maxsize;
   int size;			/* Current Size of window */
   int * active;
   Value * data;
/*   Value data[MAX_BUFFER_SIZE+1]; */ /* window buffer */ 
} buffer;


/*  buffer_init:  creates & returns a pointer to a new, empty circular array  */
buffer buffer_init (int max);


/*  isFull:  check if array is full
 *     	RETURNS:  true (1) if buffer is full (minus dummy slot); false (0) if not  */
int buffer_isFull (buffer * buf);


/*  isEmpty:  check if array is empty
 *     	RETURNS:  true (1) if buffer is empty; false (0) if not  */
int buffer_isEmpty (buffer * buf);


/*  append: adds elm at end by copying elements contents into buffer;
 *     	increments end value by 1. 
 *     	RETURNS: 1 if successful, -1 if error (array is full)   */
int buffer_append (buffer * buf, int lts, int elm);


/*  clear:  clears `num` elements from the start of the buffer 
 *      NOTE: data is never actually "cleared" since the array is static,
 *      but its slot(s) are made for available for access  
 *          Also: this is safe to call on an empty list			*/
void buffer_clear (buffer * buf, int num);


/*  get: indexing function
 *     	RETURNS: pointer to elements at the given index in array 
 *    	   NOTE: index is relative to the START position in the array */
Value * buffer_get (buffer * buf, int index);

int buffer_isActive (buffer * buf, int index);

int buffer_isStartActive (buffer * buf);

/*  put:  inserts element at given index
 *      NOTE:  Element will be inserted at index RELATIVE to start position
 * 		RETURNS:  First Open element in the buffer
 *      */
int buffer_put (buffer * buf, int lts, int elm, int index);

int buffer_size(buffer * buf);

int buffer_end(buffer * buf);
/* print: print out elements in the buffer, for debugging. */
void buffer_print(buffer * buf);
void buffer_print_select(buffer * buf);

int buffer_first_open(buffer * buf);

#endif
