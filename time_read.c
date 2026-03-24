#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"

#define COUNT 1000
char buf[512] = {1};

int
main(int argc, char *argv[])
{
  int i, fd;
  int uptime0 = uptime();
  for(i = 0; i < (COUNT + 1); i++) {
    fd = open("init", O_RDWR | O_CREATE);
    read(fd, buf, 512);
    close(fd);
  }
  int uptime1 = uptime();
  int diff1 = uptime1 - uptime0;
  // printf(1, "1000 times: %d\n", uptime1 - uptime0);

  uptime0 = uptime();
  fd = open("init", O_RDWR | O_CREATE);
  read(fd, buf, 512);
  close(fd);
  uptime1 = uptime();
  int diff2 = uptime1 - uptime0;
  printf(1, "accurate time: %d us\n", (diff1 - diff2)/COUNT);

  exit();
}
