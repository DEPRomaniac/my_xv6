#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "spinlock.h"


#define NUM_PRC 5

struct spinlock lk;

int rec_func(int l){
	if (l >= 10)
		return l;
	acquire_rec(&lk);
	printf(1, "aquire: number %d iteration\n", l );
	rec_func(l+1);
	release_rec(&lk);
	printf(1, "release: number %d iteration\n", l );
	return 0;
}


int main(){
	init_lock(&lk);

	int l = 0;
	rec_func(l);
	// pid_t pid[NUM_PRC];
	// int l = 0;
	// for (int i = 0; i < NUM_PRC; i++){
	// 	pid[i] = fork();
	// 	if (pid[i] == 0){

	// 	}
	// }



}