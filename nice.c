#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

/*
 * Ref: https://medium.com/@harshalshree03/xv6-implementing-ps-nice-system-calls-and-priority-scheduling-b12fa10494e4
 */

int
main(int argc, char *argv[])
{
  int priority, pid;
  if(argc < 3){
    printf(2,"Usage: nice pid priority:[0,3]{0 is highest priority}\n");
    exit();
  }
  pid = atoi(argv[1]);
  priority = atoi(argv[2]);
  if (priority < 0 || priority > 3){
    printf(2,"Invalid priority [0,4)!\n");
    exit();
  }
  nice(pid, priority);
  exit();
}
