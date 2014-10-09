/******************************************************************************
 *  File: buffer.h  - REF
 * 
 *  Author: Benjamin Ring
 *  Date:   9 September 2014
 *    
 *  Descirption:  The circular buffer array uses a static array with a start & end
 *    value. Elements are appended to the end and removed from the start in a FIFO manner. 
 *    Implementation provides arbitrary indexing of elements relative to the start position.
 *    In this implementation, the circular array will always contain one "dummy" slot which is never 
 *    filled, thus the actual memory used will be MAX_SIZE + 1.
 ********************************************************************************/



#ifndef BUFFER_H
#define BUFFER_H


#include "config.h"
#include "message.h"


typedef struct buffer {
   int start;
   int end;
   int open;
   int offset;
   Value data[MAX_BUFFER_SIZE+1];
} buffer;


/*  buffer_init:  creates & returns a pointer to a new, empty circular array  */
buffer * buffer_init ();


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


/*  clear:  clears all elements from the buffer & resets it
 *       NOTE: data is never actually "cleared" since the array is static,
 *         but its slot(s) are made for available for access  
 *         Also: this is safe to call on an empty list			*/
void buffer_clear_all (buffer * buf, int num);


/*  get: indexing function
 *     	RETURNS: pointer to elements at the given index in array 
 *    	   NOTE: index is relative to the START position in the array */
Value * buffer_get (buffer * buf, int index);


/*  put:  inserts element at given index
 *      NOTE:  Element will be inserted at index RELATIVE to start position
 * 		RETURNS:  First Open element in the buffer
 *      */
int buffer_put (buffer * buf, int lts, int elm, int index);

int buffer_size(buffer * buf);

int buffer_end(buffer * buf);
/* print: print out elements in the buffer, for debugging. */
void buffer_print(buffer * buf);

#endif
