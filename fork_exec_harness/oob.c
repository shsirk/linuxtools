/**
 * sample to simulate out of bound error for asan  
 */

#include <stdio.h>
#include <stdlib.h>

int main ()
{
   char *buff = malloc(10);
   
   buff[100] = 'C';

   printf("%c", buff[70]);

   free(buff);

   return 0;
}

