#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "spinlock.h"


#define NUM_PRC 2

struct spinlock lk;

int rec_func(int l, int pid){
	if (l >= 10)
		return l;
	acquire_rec(&lk);
	printf(1, "___pid %d__aquire: number %d iteration\n", pid,l );
	rec_func(l+1, pid);
	printf(1, "___pid %d__release: number %d iteration\n", pid, l);
	release_rec(&lk);
	return 0;
}


int main(){
	init_lock(&lk);
	int l = 0;
	// lk = malloc(sizeof *spinlock);;
	// printf(1,"good\n" );
	int pid[NUM_PRC];
	// int l = 0;
	for (int i = 0; i < NUM_PRC; i++){
		pid[i] = fork();
		if (pid[i] == 0){
			rec_func(l, getpid());
			exit();
		}
	}

	wait();
	wait();
	exit();
	return 0;
}