#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"

#define DEFAULT_PARENT_PRIORITY "100"

void foo_child(){
    int pid = getpid();
    int parent_priority = atoi(DEFAULT_PARENT_PRIORITY);
    change_prio(pid, parent_priority / 10 + 1);
    change_queue(pid, 3);
    printf(1, "foo process %d began exec. parent priority is %d\n", pid, parent_priority);
    sleep(pid * 100);

    double r = 0;
    while(1){
        r += 1;
        if ( r > 400000000)
            break;
    }
    // char *params[] = {"foo_child", DEFAULT_PARENT_PRIORITY};
    // if (exec("foo_child", params)  < 0)
    //     printf(1, "exec for %d failed\n", getpid());
}

int main()
{
    int parent_pid = getpid();
    change_prio( parent_pid, atoi(DEFAULT_PARENT_PRIORITY));
    change_queue( parent_pid, 3);
    for (int i = 0; 1; i++)
    {
        int p = fork();
        if (p < 0)
        {
            printf(1, "fork error\n");
            exit();
        }
        if (p == 0)
        {
            foo_child();
            exit();
        }
        if ( p > 0 && i == 6){
            wait();
            i = i-1;
        }
        sleep(50);
    }
    exit();
}