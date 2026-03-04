#include <unistd.h>                                                           
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>

#define ERR_PARAM_VIOLATION -1
#define LAST_GENERATION_DONE 0

char spacefill[] = "                    "; // empty space for pre-pending lines
int generations;

int check_and_initialize(int argc, char *argv[]) {
	if ( argc < 2) {
		return ERR_PARAM_VIOLATION;
	}
	generations = atoi(argv[1]);
	if ( generations < 1 || generations > 10 ) {
		return ERR_PARAM_VIOLATION;
	} 
	return 0;
}
int generate_family_tree_r(int generations);

int main(int argc, char *argv[]) {
	int c_i_status = check_and_initialize(argc, argv);
	if (c_i_status != 0) {
		fprintf(stderr,"Usage: tree_fork generations\n\tgenerations: int between 1 and 10\n");
		return c_i_status;
	}
	
	if ( argc == 1 ) {
		return ERR_PARAM_VIOLATION;
	}
	generations = atoi(argv[1]);
	generate_family_tree_r(1);
	return 0;
}

int spawn_and_launch(int current_generation) {	
	pid_t pid = fork(); // pid of child, or 0
	//printf("I just performed a fork. my generation is: %d", current_generation);
	if (pid==-1) {
		perror("fork failed");
	} else if (pid == 0) {
		// child process	
		generate_family_tree_r(current_generation+1);
		exit(0);
	} else {
		// parent process
		waitpid(pid, NULL, 0);
	}
	return 0;
}	

int generate_family_tree_r(int current_generation) {	
	pid_t pid;
	char spaces[21];
	//printf("current gen: %d\n\n", current_generation);
	//printf("total gen: %d\n\n", generations);
	if (current_generation > generations) {
		return LAST_GENERATION_DONE;
	}
	// print my own message before forking
	int len = 2*(current_generation-1);
	memcpy(spaces, spacefill, len);
	spaces[len] = '\0';
	pid = getpid();
	printf("%sGeneration: %d | PID: %d\n", spaces, current_generation, pid); //fix
	fflush(stdout);
	spawn_and_launch(current_generation);	
	spawn_and_launch(current_generation);	
	return 0;
}


