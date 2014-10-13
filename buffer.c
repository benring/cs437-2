/******************************************************************************
 *  File:  buffer.c - REF
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
buffer buffer_init ()
{
	int i;
	buffer buf;
	buf.start = 0;
	buf.end = 0;
	buf.open = 0;
	buf.offset = 0;
	buf.upperlimit = 0;
	buf.size = 0;
	for (i=0; i<=MAX_BUFFER_SIZE; i++) {
		buf.data[i].active = INACTIVE;
	}
	return buf;
}

/*  isFull:  check if array is full
 *   *  	RETURNS:  true (1) if array is full (minus dummy slot); false (0) if not  */
int buffer_isFull (buffer * buf)
{
	return (buf->size == MAX_BUFFER_SIZE);
/*	((buf->end + 1) % (MAX_BUFFER_SIZE+1) == buf->start);  */
}

/*  isEmpty:  check if array is empty
 *   *  	RETURNS:  true (1) if circular array is empty; false (0) if not  */
int buffer_isEmpty (buffer * buf)
{
	return (buf->size == 0);
/*	return (buf->end == buf->start);  */
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
		buf->size++;
		buf->upperlimit++;
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
 *       NOTE: data is never actually "cleared" since the array is statiFc,
 *          but its slot(s) are made for available for access
 *           Also: this is safe to call on an empty list			
 * 		TODO: check for efficiency*/
void buffer_clear (buffer * buf, int num)
{
	int i, j;
	i = (buf->start + num) % (MAX_BUFFER_SIZE+1);
	for (j=0; j<num; j++) {
		buf->data[(buf->start + j) % (MAX_BUFFER_SIZE+1)].active = INACTIVE;
	}
	buf->start = i;
	buf->offset += num;
	buf->size -= num;

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
	buf->size = 0;
	buf->offset = buf-> upperlimit;
}




/*  get: indexing function
 *   	RETURNS: pointer to elements at the given index in array
 *     	   NOTE: index is relative to the START position in the array 
 * 
 * 		TODO:  CHECK FOR ACTIVE DATA VALUE  */
Value * buffer_get (buffer * buf, int index)
{
	if (index > buf->upperlimit)  {
		printf("ERROR! Index out of bounds on get %d", index);
		return 0;
	}
	return &(buf->data[(buf->start + index - buf->offset) % (MAX_BUFFER_SIZE + 1)]);
}

int buffer_isActive (buffer * buf, int index)  {
	Value elm;
	if (index > buf->upperlimit) {
		return FALSE;
	}
	elm = buf->data[(buf->start + index - buf->offset) % (MAX_BUFFER_SIZE + 1)];
	return (elm.active == ACTIVE);
}



/*  put: inserts element at given index  */
int buffer_put (buffer * buf, int lts, int elm, int index)
{
	Value val;
	int local_index;
	
	/* Check for Index OOB & return error */
	if (index > (buf->offset + MAX_BUFFER_SIZE+1)) {
		printf("ERROR! Index out of bounds\n");
		return -1;
	}

	/*  Create the data element from input vals  */
	val.lts = lts;
	val.data = elm;
	val.active = ACTIVE;
	
	local_index = (buf->start + (index - buf->offset)) % (MAX_BUFFER_SIZE + 1);
	if (buf->data[local_index].active == INACTIVE) {
		buf->size++;
	}
	buf->data[local_index] = val;
	
	/*  UPDATE "open": If inserting into current open slot, update to next open pos */
	if (local_index == buf->open)
	{
		/*  FIND next open slot  */
		do  {
			buf->open++;
			if (buf->open > MAX_BUFFER_SIZE)
				buf->open = 0;
		} while (buf->data[buf->open].active == ACTIVE);
	}
	
	/*  UPDATE "end": IF inserting at or beyond current end position */
	if (index >= buf->upperlimit)  {
		buf->upperlimit = index;
		buf->end = local_index + 1;
		if (buf->end > MAX_BUFFER_SIZE) {
			buf->end = 0;
		}
	}
/*	if (local_index == buf->end) {
		buf->end++;
		buf->upperlimit++;
		if (buf->end > MAX_BUFFER_SIZE)
			buf->end = 0;
	}
*/	
	/*  RETURN:  First "open" slot in the Buffer */
	if (buf->open >= buf->start) {
		return (buf->open - buf->start + buf->offset);
	}
	else {
		return (buf->open + MAX_BUFFER_SIZE + 1 - buf->start + buf->offset);
	}
}

void buffer_print(buffer * buf) {
  int i = buf->offset;
  int j; 
  for(i = 0; i < MAX_BUFFER_SIZE + 1; i++) {
    printf("%3d", i + buf->offset); 
  }
  printf("\n");

  j = buf->start;
  for(i = 0; i < MAX_BUFFER_SIZE+1; i++) {
    if (j > MAX_BUFFER_SIZE) {
      j = 0;
    }
    if(buf->data[j].active == ACTIVE) {
      printf("%3d", buf->data[j].lts);
    }
    else {
      printf("%3c", 'x');
    }
    j++;
  }
  printf("\n");
}

int buffer_size(buffer * buf)  {
	return (buf->size);
/*	if (buf->end >= buf->start)  {
		return (buf->end - buf->start);
	}
	else  {
		return (buf->end - buf->start + MAX_BUFFER_SIZE + 1);
	}
*/}


int buffer_end(buffer * buf)  {
	return buf->upperlimit;
/*	printf("START=%d,  END=%d  OPEN=%d\n", buf->start, buf->end, buf->open);
	if (buf->end >= buf->start)  {
		return (buf->end + buf->offset - 1);
	}
	else  {
		return (buf->end + MAX_BUFFER_SIZE + buf->offset);
	}
*/	
}

int buffer_first_open(buffer * buf) {
	if (buf->open >= buf->start)  {
		return (buf->offset + (buf->open - buf->start));
	}
	else  {
		return (buf->offset + (MAX_BUFFER_SIZE + buf->open - buf->start));
	}
	
}




