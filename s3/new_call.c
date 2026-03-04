#include <unistd.h>                                                                   
#include <asm/unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

int main(int argc, char * argv[]) {
	uid_t uid;
	int return_val;
	uid = syscall(__NR_noargs);
	printf("initially, uid is: %d\n",uid);
	if (argc > 6)
		return_val = syscall(__NR_onearg,0);
	else
		return_val = syscall(__NR_onearg,atoi(argv[1]));
	//return_val = setuid(0);
	if ( return_val != 0 ) printf("Error: setuid failed! Reason: %s\n", strerror(errno));
	uid = syscall(__NR_noargs);
	printf("now, uid is: %d\n", uid);
	return 0;
}
