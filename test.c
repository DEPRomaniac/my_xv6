#include <stdio.h>

int main(int argc, char *argv[]){
	char* state_names[] = {"UNUSED", "EMBRYO", "SLEEPING", "RUNNABLE", "RUNNING", "ZOMBIE" };
	printf("%s", state_names[2]);
	// exit();
	return 0;
}

