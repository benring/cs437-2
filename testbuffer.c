#include "buffer.c"

void printbuf(buffer *b) {
	printf("  START= %d,\tEND= %d,\tOPEN= %d,\tOFF= %d,\n", b->start, b->end, b->open, b->offset);
}

int main()  {
	buffer b;
	Value v;
	int a; 
	
	printf("\n\nTEST\n-----\n");
	
	b = buffer_init(6);
	printbuf(&b);
	printf("ISEMPTY: %d\n", buffer_isEmpty(&b));
	buffer_append(&b, 1, 1);
	printbuf(&b);
	printf("ISEMPTY: %d\n", buffer_isEmpty(&b));
	
	v = *buffer_get(&b, 0);
	printf("VAL->  ACTIVE:%d\tLTS:%d\tDATA:%d\n", b.active[0], v.lts, v.data);
	v = *buffer_get(&b, 1);
	printf("VAL->  ACTIVE:%d\tLTS:%d\tDATA:%d\n", b.active[1], v.lts, v.data);
	
	a = buffer_put(&b, 4, 2342, 4);
	printf("PUT 2342 at 4.  NEXT OPEN SLOT:%d\n", a);
	printbuf(&b);

	a = buffer_put(&b, 6, 33333, 3);
	printf("PUT 3333 at 3.  NEXT OPEN SLOT:%d\n", a);
	printbuf(&b);
	
	a = buffer_put(&b, 8, 1111, 1);
	printf("PUT 1111 at 1.  NEXT OPEN SLOT:%d\n", a);
	/*printbuf(&b);*/
	printf("[: \n");
	a = buffer_put(&b, 23, 2222, 2); 
	printf("]\n");
	printf("PUT 2222 at 2.  NEXT OPEN SLOT:%d\n", a);
	printbuf(&b);

	a = buffer_put(&b, 23, 555, 5);
	printf("PUT 555 at 5.  NEXT OPEN SLOT:%d\n", a);
	printbuf(&b);
	a = buffer_put(&b, 23, 666, 6);
	printf("PUT 666 at 6.  NEXT OPEN SLOT:%d\n", a);
	printbuf(&b);
	printf("  ISFULL: %d\n", buffer_isFull(&b));
	a = buffer_put(&b, 23, 7777, 7);
	printf("PUT 777 at 7.  NEXT OPEN SLOT:%d\n", a);
	printbuf(&b);
	printf("  ISFULL: %d\n", buffer_isFull(&b));
	a = buffer_put(&b, 23, 8888, 8);
	printf("PUT 8888 at 8.  NEXT OPEN SLOT:%d\n", a);
	printbuf(&b);
	printf("  ISFULL: %d\n", buffer_isFull(&b));

	buffer_clear(&b, 2);
	printf("CLEAR: 2\n");
	printbuf(&b);
	printf("  ISFULL: %d\n", buffer_isFull(&b));
	a = buffer_put(&b, 23, 8888, 8);
	printf("PUT 8898 at 8.  NEXT OPEN SLOT:%d\n", a);
	printbuf(&b);
	return 0;

}