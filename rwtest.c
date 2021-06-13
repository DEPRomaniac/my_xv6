#include "types.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char** argv)
{
	int pattern = atoi(argv[1]);
	if(pattern < 2){
		printf(1, "invalid pattern\n");
		exit();
	}
	rwinit();

	char pstr[100] = "";
	int plen = 0;
	int temp_pattern = pattern;
	while (temp_pattern / 2 >= 1){
	    pstr[plen++] = (temp_pattern%2) + '0';
	    temp_pattern /= 2;
  	}
	pstr[plen] = '\0';
	// cprintf("%s", pstr);
	char temp;
	for (int i = 0; i < plen/2; i++){
		temp = pstr[i];
		pstr[i] = pstr[plen-1-i];
		pstr[plen-1-i] = temp;
	}
	printf(1, "%s\n", pstr);

	int pid[30];
	for (int i = 0; i < plen; i++){
		pid[i] = fork();
		if (pid[i] == 0){
			rwtest ( (pstr[i]) - '0', 0);
			exit();
		}
		// sleep(1);
	}

	for (int i = 0; i<plen; i++)
		wait();

	printf(1,"user program finished\n");
	exit();
}