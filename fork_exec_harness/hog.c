/*
	sample to generate app hang, 
	killall generate SIGTERM (caught by fork_harness)
	should use SIGKILL with kill -9 `pidof app` to avoid fork_harness to catch it (timeout stuff)
 */
#include <stdio.h>
#include <unistd.h>

int main()
{
	while (1) {
		;;
		usleep(5*1000);
	}
	return 0;
}

