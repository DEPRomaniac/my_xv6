#include "types.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char* argv[])
{
    if (argc<3)
    {
        printf(1, "invalid arguments\n");
        exit();
    }
    change_prio(atoi(argv[1]), atoi(argv[2]));
    exit();
}