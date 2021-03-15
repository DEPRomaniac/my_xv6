#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
   
char* strdiff(char str1[], char str2[], int length)
{
    char *ans = malloc(length);
    for(int i=0; i<length; i++){
        if(str1[i] <str2[i]){
            ans[i] = '1';
        }
        else
            ans[i] = '0';       
    }
    return ans;
}

void save(char res[], int length)
{
	int fd;
	fd = open("strdiff_result.txt", O_CREATE | O_RDWR);
    for(int i=0; i<length; i++){
        write(fd, &res[i], 1);
    }
    write(fd, "\n", 1);
    close(fd);
}

int main(int argc, char *argv[])
{
    int length1 = strlen(argv[1]);
    int length2 = strlen(argv[2]);

    if(length1>length2){
        char *func_res = malloc(length2);
        char *res = malloc(length1);
        func_res = strdiff(argv[1], argv[2], length2);
        for(int i=0; i<length2; i++)
            res[i] = func_res[i];
        for(int i=0; i<length1-length2; i++)
            res[length1-i-1] = '0';
        save(res, length1);
    }
    else if(length1< length2){
        char *func_res = malloc(length1);
        char *res = malloc(length2);
        func_res = strdiff(argv[1], argv[2], length1);
        for(int i=0; i<length1; i++)
            res[i] = func_res[i];
        for(int i=0; i<length2-length1; i++)
            res[length2-i-1] = '1';
        save(res, length2);
    }
    else if(length1 == length2){
        char *func_res = malloc(length1);
        char *res = malloc(length2);
        func_res = strdiff(argv[1], argv[2], length1);
        for(int i=0; i<length1; i++)
            res[i] = func_res[i]; 
        
        save(res, length2); 
    }
    exit();
}

