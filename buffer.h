/******************************************************************************
 *  File: circarray.h
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



typedef struct buffer {
   int start;
   int end;
   unsigned char * buffer[MAX_SIZE+1][SLOT_SIZE];
} circarray;


/*  circarr_init:  creates & returns a pointer to a new, empty circular array  */
circarray * circarray_init ();


/*  isFull:  check if array is full
 *     	RETURNS:  true (1) if array is full (minus dummy slot); false (0) if not  */
int circarray_isFull (circarray * carr);


/*  isEmpty:  check if array is empty
 *     	RETURNS:  true (1) if circular array is empty; false (0) if not  */
int circarray_isEmpty (circarray * carr);


/*  append: adds elm at end by copying elements contents into buffer;
 *     	increments end value by 1. 
 *     	RETURNS: 1 if successful, -1 if error (array is full)   */
int circarray_append (circarray * carr, unsigned char * elm );


void circarray_grow (circarray * carr);
/*  clear:  clears `num` elements from the start of the buffer 
 *      NOTE: data is never actually "cleared" since the array is static,
 *      but its slot(s) are made for available for access  
 *          Also: this is safe to call on an empty list			*/
void circarray_clear (circarray * carr, int num);


/*  clear:  clears all elements from the buffer & resets it
 *       NOTE: data is never actually "cleared" since the array is static,
 *         but its slot(s) are made for available for access  
 *         Also: this is safe to call on an empty list			*/
void circarray_all (circarray * carr, int num);


/*  get: indexing function
 *     	RETURNS: pointer to elements at the given index in array 
 *    	   NOTE: index is relative to the START position in the array */
unsigned char * circarray_get (circarray * carr, int index);



/*  put:  inserts element at given index
 *      NOTE:  Element will be inserted at index RELATIVE to start position
 *      */
void circarray_put (circarray * carr, unsigned char * elm, int index);



/*  get_copy:  indexing function 
 *   *  	RETURNS: copy of the element at the given index in array
 *    *  	   NOTE: index is relataive to the START position    */
unsigned char * circarray_get_copy (circarray * carr, int index);


/*  size: Returns current size of the arraay  */
int circarray_size (circarray * carr);


#endif
