/******************************************************************************
 * File: linkedlist.c
 *
 * Author: Benjamin Ring
 * Date:   9 September 2014
 *
 * Descirption:  See linkedlist.h, as well as linear search.
 *
 *  To implement: Update comparison operation (in search)
 *
 *****************************************************************************/

#include "linkedlist.h"


/*  isEmpty:  returns true if the list is empty  */
int isEmpty (linked_list * head)  {
    return (head == NULL_PTR);
}

/*  enq (EnQueue):  Adds one element to the head of the list */
void enq (linked_list ** head, generic val)  {
	linked_list *elm = malloc (sizeof(linked_list));
	elm->data = val;
	elm->next = *head;
	*head = elm;
}

/*  deq (DeQueue):  Removes one element to the end of the list 
 * 			and returns its data value as an int
 * 	(requires sequential search to reach tail)			*/
generic deq (linked_list ** head)  {
	linked_list *ptr = *head;
	linked_list *prev = NULL_PTR;
	generic value;
	if (ptr == NULL_PTR)  {
		printf("NULL Pointer Error");
		exit(1);
	}
	while (ptr->next != NULL_PTR) {
		prev = ptr;
		ptr = ptr->next;
	}
	value = ptr->data;
	if (prev == NULL_PTR)
		*head = NULL_PTR;
	else
		prev->next = NULL_PTR;
	free(ptr);
	return value;
}


/*  search:  Simple linear search to find given val (O(n) log op)
 *          RETURN:  1 if found, 0 if not found  */
int search (linked_list * head, generic val)  {
    return 0;
}




