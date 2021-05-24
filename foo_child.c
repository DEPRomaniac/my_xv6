#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"

#define CALC 400000

int main(int argc, char* argv[]){
	int pid = getpid();
	int parent_priority = atoi(argv[1]);
	// change_prio(pid, parent_priority - 1);

	printf(1, "HELLO I process %d began exec. my parent_priority is %d\n", pid, parent_priority);
	sleep(pid * 100);

	float r = 0;
	while(1){
		r += 1;
		if ( r > CALC)
			break;
	}
	exit();
}