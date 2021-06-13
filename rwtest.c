#include "types.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char** argv)
{
	uint pattern = atoi(argv[1])
	if(pattern < 2){
		printf(1, "invalid pattern");
		exit();
	}
	rwinit();
	rwtest(pattern);
	exit();
}