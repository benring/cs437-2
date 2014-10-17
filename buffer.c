/******************************************************************************
 *  File:  buffer.c
 *
 *  Author: Benjamin Ring & Josh Wheeler
 *  Date:   15 October 2014
 *
 *  Description:  Buffer Implemention. See buffer.h for full description.
 *******************************************************************************/
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "buffer.h"


/*  buffer_init:  creates & returns a pointer to a new, empty buffer  */
buffer buffer_init (int max)
{
	int i;
	buffer buf;
	buf.start = 0;
	buf.end = 0;
	buf.open = 0;
	buf.offset = 0;
	buf.upperlimit = 0;
	buf.maxsize = max;
	buf.size = 0;
	buf.active = malloc (max * sizeof(int));
	buf.data = malloc (max * sizeof(Value));
	for (i=0; i<max; i++) {
		buf.active[i] = INACTIVE;
	}
	return buf;
}

/*  isFull:  check if array is full
 *    RETURNS:  true (1) if array is full (minus dummy slot); false (0) if not  */
int buffer_isFull (buffer * buf)
{
	return (buf->size == buf->maxsize);
}

/*  isEmpty:  check if array is empty
 *    RETURNS:  true (1) if circular array is empty; false (0) if not  */
int buffer_isEmpty (buffer * buf)
{
	return (buf->size == 0);
}


/*  append: adds elm at end by copying elements contents into buffer;
 *   increments end value by 1.
 *     RETURNS: 1 if successful, -1 if error (array is full)   */
int buffer_append (buffer * buf, int lts, int elm)
{
	Value val;
	if (buffer_isFull(buf))
		return -1;
	else {
		val.lts = lts;
		val.data = elm;
		buf->active[buf->end] = ACTIVE;
		buf->data[buf->end] = val;
		buf->size++;
		buf->upperlimit++;
		if (buf->open == buf->end) {
			buf->open += 1;
			if (buf->open >= buf->maxsize)
				buf->open = 0;
		}
		buf->end += 1;
		if (buf->end >= buf->maxsize)
			buf->end = 0;
	}
	return 1;
}


/*  clear:  clears `num` elements from the start of the buffer
 *       NOTE: data is never actually "cleared" since the array is statiFc,
 *          but its slot(s) are made for available for access
 *           Also: this is safe to call on an empty list	*/
 void buffer_clear (buffer * buf, int num)
{
	int i, j;
	i = (buf->start + num) % (buf->maxsize);
	for (j=0; j<num; j++) {
		buf->active[(buf->start + j) % (buf->maxsize)] = INACTIVE;
	}
	buf->start = i;
	buf->offset += num;
	buf->size -= num;
}


/*  get: indexing function
 *   	RETURNS: pointer to elements at the given index in array
 *     	   NOTE: index is relative to the START position in the array 
 * 
 * 		TODO:  CHECK FOR ACTIVE DATA VALUE  */
Value * buffer_get (buffer * buf, int index)
{
	if (index > buf->upperlimit)  {
		printerr("ERROR! Index out of bounds on get %d", index);
		return 0;
	}
	return &(buf->data[(buf->start + index - buf->offset) % (buf->maxsize)]);
}

/*
 *  isActive:  Checks to see if the index position is "Active" (in use) */
int buffer_isActive (buffer * buf, int index)  {
	if (index > buf->upperlimit) {
		return FALSE;
	}
	return (buf->active[(buf->start + index - buf->offset) % (buf->maxsize)] ==  ACTIVE);
}
 /*  Special case for checking first element in the buffer */
int buffer_isStartActive (buffer * buf)  {
	return (buf->active[buf->start] ==  ACTIVE);
}



/*  put: insert operation
 * 		Inserts element at given index and sets data values 
 * 		Checks and, if needed, updates the end and open position
			as well as upperlimit  */
int buffer_put (buffer * buf, int lts, int elm, int index)
{
	Value val;
	int local_index;
	/* Check for Index OOB & return error */
	if (index > (buf->offset + buf->maxsize)) {
		printerr("ERROR! Index out of bounds\n");
		return -1;
	}

	/*  Create the data element from input vals  */
	val.lts = lts;
	val.data = elm;
	local_index = (buf->start + (index - buf->offset)) % (buf->maxsize);
	if (buf->active[local_index] == INACTIVE) {
		buf->size++;
	}
	buf->data[local_index] = val;
	buf->active[local_index] = ACTIVE;
	
	/*  UPDATE "open": If inserting into current open slot, update to next open pos */
	if (local_index == buf->open)
	{
		/*  FIND next open slot  */
		if (buf->size == buf->maxsize) {
			buf->open = buf->start;
		}
		else {
			do  {
				buf->open++;
				if (buf->open >= buf->maxsize)
					buf->open = 0;
			} while (buf->active[buf->open] == ACTIVE);
		}
	}

	/*  UPDATE "end": IF inserting at or beyond current end position */
	if (index >= buf->upperlimit)  {
		buf->upperlimit = index;
		buf->end = local_index + 1;
		if (buf->end >= buf->maxsize) {
			buf->end = 0;
		}
	}
	/*  RETURN:  First "open" slot in the Buffer */
	if (buf->open >= buf->start) {
		return (buf->open - buf->start + buf->offset);
	}
	else {
		return (buf->open + buf->maxsize - buf->start + buf->offset);
	}
}

/*  print: display buffer LTS value (useful for debugging) */
void buffer_print(buffer * buf) {
  int i = buf->offset;
  int j; 
  for(i = 0; i < buf->maxsize; i++) {
    printdb("%5d", i + buf->offset); 
  }
  printdb("\n");

  j = buf->start;
  for(i = 0; i < buf->maxsize; i++) {
    if (j >= buf->maxsize) {
      j = 0;
    }
    if(buf->active[j] == ACTIVE) {
      printdb("%5d", buf->data[j].lts);
    }
    else {
      printdb("%5c", 'x');
    }
    j++;
  }
  printdb("\n");
}

/*  print: display buffer LTS value (useful for debugging) */
void buffer_print_select(buffer * buf) {
  int i = buf->offset;
  int j; 
  for(i = 0; i < buf->maxsize; i++) {
    printsel("%5d", i + buf->offset); 
  }
  printsel("\n");

  j = buf->start;
  for(i = 0; i < buf->maxsize; i++) {
    if (j >= buf->maxsize) {
      j = 0;
    }
    if(buf->active[j] == ACTIVE) {
      printsel("%5d", buf->data[j].lts);
    }
    else {
      printsel("%5c", 'x');
    }
    j++;
  }
  printsel("\n");
}


int buffer_size(buffer * buf)  {
	return (buf->size);
}

int buffer_end(buffer * buf)  {
	return buf->upperlimit;
}

int buffer_first_open(buffer * buf) {
	if (buf->open >= buf->start)  {
		return (buf->offset + (buf->open - buf->start));
	}
	else  {
		return (buf->offset + (buf->maxsize + buf->open - buf->start));
	}
}

