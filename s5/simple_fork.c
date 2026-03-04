#include <unistd.h>                                                           
#include <stdio.h>
//#include <string.h>
#include <errno.h>
//#include <stdlib.h>

int main() {
	printf("This is the parent, pre-fork, doing a print\n");
	pid_t pid = fork();
	if (pid==-1) {
		perror("fork failed");
	} else if (pid == 0) {
		// child process
		pid = getpid();
		pid_t ppid = getppid();
		printf("I'm so childish. I'm childing everywhere.\tPID=%u\tPPID=%u\n",pid,ppid);
	} else {
		// parent process
		printf("I awaken to find that now, I am a parent. \tChild PID=%u\n",pid);
	}
	return 0;
}
