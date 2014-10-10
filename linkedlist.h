/******************************************************************************
 * File: linkedlist.h
 *
 * Author: Benjamin Ring
 * Date:   9 September 2014
 *
 * Descirption:  Simple implementation of a linked list. Elements of the list
 * 	consist of an << generic >> data and a pointer to element. Implementation
 *  provides both EnQueue and DeQueue opertions to add & remove elements in
 *  a FIFO format, as well as linear search.
 *
 *  To implement, update the following to data type of choice (vice int):
 *    typedef int generic   ==>   typedef [TYPE] generic
 *
 *****************************************************************************/

#ifndef LINKED_LIST_H
#define LINKED_LIST_H


#define NULL_PTR 0

typedef int generic;

typedef struct element linked_list;

struct element {
	generic data;
	struct element *next;
};


/*  isEmpty:  returns true if the list is empty  */
int isEmpty (linked_list * head);


/*  enq (EnQueue):  Adds one element to the head of the list */
void enq (linked_list ** head, generic val);

/*  deq (DeQueue):  Removes one element to the end of the list
 * 			and returns its data value as an int
 * 	(requires sequential search to reach tail)			*/
generic deq (linked_list ** head);


int search (linked_list * head, generic val);


#endif
