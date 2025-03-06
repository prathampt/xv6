#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

int f(void);

int
main(int argc, char *argv[])
{
    f();
    exit();
}

int f() {
    int cpid;
    cpid = fork();
    if(cpid == 0) {
        fork();
        fork();
        while(1);
    }
    else {
        hello();
    }
}
