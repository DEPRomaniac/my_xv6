#include "types.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char** argv)
{
	// create processes
	int num_of_processes = atoi(argv[1]);

	if(num_of_processes < 1)
	{
		printf(1,"enter valid number of processes\n");
		return 0;
	}

	int pid;

	printf(1,"These additional processes have been created for testing purposes\n");
	for (int i = 0; i <num_of_processes; i++){
		pid = fork();
		if(pid == -1)
			return 0;
		if(pid == 0)
		{
			printf(1,"new process created -> pid = %d\n", getpid());
		}
		else 
		{
			wait();
			break;
		}
	}
	// sleep(10);
	exit();
}