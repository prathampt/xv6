#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"

#define COUNT 1000
int
main(int argc, char *argv[])
{
  int pid, i;
  int uptime0 = uptime();
  for(i = 0; i < (COUNT + 1); i++) {
    close(open("README", O_RDWR));
  }
  int uptime1 = uptime();
  int diff1 = uptime1 - uptime0;
  // printf(1, "1000 times: %d\n", uptime1 - uptime0);

  uptime0 = uptime();
  close(open("README", O_RDWR));
  uptime1 = uptime();
  int diff2 = uptime1 - uptime0;
  printf(1, "accurate time: %d us\n", (diff1 - diff2)/COUNT);

  exit();
}
