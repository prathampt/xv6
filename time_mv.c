#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"

#define COUNT 1000
int
main(int argc, char *argv[])
{
  int pid, i;
  open("tmp1", O_CREATE);
  int uptime0 = uptime();
  for(i = 0; i < (COUNT + 1); i++) {
    if(i%2) {
      link("tmp", "tmp1");
      unlink("tmp");
    }
    else {
      link("tmp1", "tmp");
      unlink("tmp1");
    }
  }
  int uptime1 = uptime();
  int diff1 = uptime1 - uptime0;
  // printf(1, "1000 times: %d\n", uptime1 - uptime0);

  uptime0 = uptime();
  if((COUNT + 1)%2) {
    link("tmp", "tmp1");
    unlink("tmp");
  }
  else {
    link("tmp1", "tmp");
    unlink("tmp1");
  }
  uptime1 = uptime();
  int diff2 = uptime1 - uptime0;
  printf(1, "accurate time: %d us\n", (diff1 - diff2)/COUNT);

  unlink("tmp");
  unlink("tmp1");
  exit();
}
