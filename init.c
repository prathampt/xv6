// init: The initial user-level program

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "param.h"

char *argv[] = { "sh", 0 };
char *stress_argv[] = { "realstress", "4", 0 };
char *argv_idle[] = { "idle", 0 };

int
main(void)
{
  int pid, wpid, i;

  if(open("console", O_RDWR) < 0){
    mknod("console", 1, 1);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr

  // Start idle tasks
  for(i = 0; i < NCPU; i++) {
    if((pid = fork()) == 0)
      exec("idle", argv_idle);
    else if(pid < 0)
      dopanic("fork failed\n");
    else
      continue;
  }

  printf(1, "init: started %d idle tasks\n", NCPU);
  for(;;){
    printf(1, "init: starting sh\n");
    pid = fork();
    if(pid < 0){
      printf(1, "init: fork failed\n");
      exit();
    }
    if(pid == 0){
      exec("sh", argv);
      // exec("realstress", stress_argv);
      printf(1, "init: exec sh failed\n");
      exit();
    }
    while((wpid=wait()) >= 0 && wpid != pid)
      printf(1, "zombie!\n");
    // dopanic("stressfs failed\n");
  }
}
