#include "types.h"
#include "user.h"
#include "fcntl.h"

// #define BRANCH_NUM 2

int main(int argc, char** argv)
{
	// int process_id = atoi(argv[1]);
	int process_id = 0;

	int num_of_processes = atoi(argv[1]);

	if(num_of_processes < 1)
	{
		printf(1,"ERROR: please enter valid number of processes\n");
		exit();
		return 0;
	}

	int pid = -1;
	// int count = BRANCH_NUM;
	int root_process_id = getpid();

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
		if (i == num_of_processes-1)
			process_id = getpid();
	}
	// sleep(1);
	if ( process_id == getpid() )
	{
		printf(1, "running get_descendant for process PID = %d\n", root_process_id);
		descendant(root_process_id);
	}

	exit();
}