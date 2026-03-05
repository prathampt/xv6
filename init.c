// init: The initial user-level program

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "test.h"

char *argv[] = { "sh", 0 };
char *argv_test[] = { "usertests", 0 };

int
main(void)
{
  int pid, wpid;

  if(open("console", O_RDWR) < 0){
    mknod("console", 1, 1);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr

  for(;;){
    if(TESTING)
      printf(1, "init: starting usertests\n");
    else
      printf(1, "init: starting sh\n");
    pid = fork();
    if(pid < 0){
      printf(1, "init: fork failed\n");
      exit();
    }
    if(pid == 0){
      if(TESTING) {
        exec("usertests", argv_test);
        printf(1, "init: exec usertests failed\n");
        exit();
      }
      else {
        exec("sh", argv);
        printf(1, "init: exec sh failed\n");
      }
      exit();
    }
    while((wpid=wait()) >= 0 && wpid != pid)
      ;

    // exit() so that kernel panics
    if(TESTING) {
      printf(1, "usertests failed\n");
      exit();
    }
  }
}
