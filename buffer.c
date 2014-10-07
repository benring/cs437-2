/******************************************************************************
 *  File:  buffer.c
 *
 *  Author: Benjamin Ring
 *  Date:   9 September 2014
 *
 *  Descirption:  The circular buffer array uses a static array with a start & end
 *   value. Elements are appended to the end and removed from the start in a FIFO manner.
 *   Implementation provides arbitrary indexing of elements relative to the start position.
 *   In this implementation, the circular array will always contain one "dummy" slot which is never
 *   filled, thus the actual memory used will be MAX_BUFFER_SIZE + 1.
 *******************************************************************************/
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "buffer.h"


/*  buffer_init:  creates & returns a pointer to a new, empty buffer  */
buffer * buffer_init ()
{
	buffer *buf = malloc (sizeof(buffer));
	buf->start = 0;
	buf->end = 0;
	buf->open = 0;
	buf->offset = 0;
	return buf;
}

/*  isFull:  check if array is full
 *   *  	RETURNS:  true (1) if array is full (minus dummy slot); false (0) if not  */
int buffer_isFull (buffer * buf)
{
	return ((buf->end + 1) % (MAX_BUFFER_SIZE+1) == buf->start);
}

/*  isEmpty:  check if array is empty
 *   *  	RETURNS:  true (1) if circular array is empty; false (0) if not  */
int buffer_isEmpty (buffer * buf)
{
	return (buf->end == buf->start);
}


/*  append: adds elm at end by copying elements contents into buffer;
 *   *  	increments end value by 1.
 *    *  	RETURNS: 1 if successful, -1 if error (array is full)   */
int buffer_append (buffer * buf, int lts, int elm)
{
	Value val;
	if (buffer_isFull(buf))
		return -1;
	else {
		val.lts = lts;
		val.data = elm;
		val.active = ACTIVE;
		buf->data[buf->end] = val;
		if (buf->open == buf->end) {
			buf->open += 1;
			if (buf->open > MAX_BUFFER_SIZE)
				buf->open = 0;
		}
		buf->end += 1;
		if (buf->end > MAX_BUFFER_SIZE)
			buf->end = 0;
	}
	return 1;
}


/*  clear:  clears `num` elements from the start of the buffer
 *       NOTE: data is never actually "cleared" since the array is static,
 *          but its slot(s) are made for available for access
 *           Also: this is safe to call on an empty list			
 * 		TODO: check for efficiency*/
void buffer_clear (buffer * buf, int num)
{
	int i, j;
	i = (buf->start + num) % (MAX_BUFFER_SIZE+1);
	for (j=0; j<num; j++) {
		buf->data[(buf->start + num) % (MAX_BUFFER_SIZE+1)].active = INACTIVE;
	}
	buf->start = i;
	buf->offset += num;
	
}


/*  clear:  clears all elements from the buffer & resets it
 *   *    NOTE: data is never actually "cleared" since the array is static,
 *    *      but its slot(s) are made for available for access
 *     *      Also: this is safe to call on an empty list			*/
void buffer_clear_all (buffer * buf, int num)
{
	buf->start = 0;
	buf->end = 0;
	buf->open = 0;
}




/*  get: indexing function
 *   	RETURNS: pointer to elements at the given index in array
 *     	   NOTE: index is relative to the START position in the array */
Value * buffer_get (buffer * buf, int index)
{
	return &(buf->data[(buf->start + index - buf->offset) % (MAX_BUFFER_SIZE + 1)]);
}




/*  put: inserts element at given index  */
int buffer_put (buffer * buf, int lts, int elm, int index)
{
	Value val;
	int rel_index;
	
	if (buffer_isFull(buf)) {
		return -1;
	}
	
	val.lts = lts;
	val.data = elm;
	val.active = ACTIVE;
	rel_index = (buf->start + (index - buf->offset)) % (MAX_BUFFER_SIZE + 1);
	buf->data[rel_index] = val;
	if (rel_index == buf->open)
	{
		do  {
			buf->open++;
			if (buf->open > MAX_BUFFER_SIZE)
				buf->open = 0;
		} while (buf->data[buf->open].active == ACTIVE);
	}
	if (rel_index == buf->end) {
		buf->end += 1;
		if (buf->end > MAX_BUFFER_SIZE)
			buf->end = 0;
	}
	return ((buf->open - buf->start) % ((MAX_BUFFER_SIZE+1) + buf->offset));
}


