#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"


char *exit_args[] = {"exit", 0};

#define COUNT 10
int
main(int argc, char *argv[])
{
  int i;
  int uptime0 = uptime();
  for(i = 0; i < (COUNT + 1); i++) {
    if(fork())
      wait();
    else
      exec("exit", exit_args);
  }
  int uptime1 = uptime();
  int diff1 = uptime1 - uptime0;
  // printf(1, "1000 times: %d\n", uptime1 - uptime0);

  uptime0 = uptime();
  if(fork())
    wait();
  else
    exec("exit", exit_args);

  uptime1 = uptime();
  int diff2 = uptime1 - uptime0;
  printf(1, "accurate time: %d us\n", (diff1 - diff2)/COUNT);

  exit();
}
