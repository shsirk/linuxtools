/*
	sample crash Seg fault for fork_harness 
 */
#include <stdio.h>

int main()
{
		int *p=0;
		*p = 0xdeadbeef;
}

