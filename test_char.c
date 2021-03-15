#include <stdio.h>

int main(){
	char c;
	while (c = getc(stdin))
		printf("%c\n", c);
}