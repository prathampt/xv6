#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"

char *argv_shutdown[] = { "shutdown", 0 };

int
main(int argc, char *argv[])
{
  if(argc != 2) {
    printf(1, "usage: realstress nthreads\n");
    exit();
  }

  int i, nthreads;
  nthreads = atoi(argv[1]);

  if(nthreads < 1 || nthreads > 4) {
    printf(1, "invalid number of threads, 1 <= nthreads <= 4\n");
    exit();
  }

  char *argv1[] = { "realstress1", argv[1], 0 };
  char *argv2[] = { "realstress2", argv[1], 0 };

  printf(1, "realstress starting\n");

  if(open("usertests.ran", 0) >= 0){
    printf(1, "already ran user tests -- rebuild fs.img\n");
    exit();
  }
  close(open("usertests.ran", O_CREATE));

  int uptime0 = uptime();

  if(fork() == 0) {
    exec("realstress1", argv1);
  }
  wait();
  if(fork() == 0) {
    exec("realstress2", argv2);
  }
  wait();
  printf(1, "Total time: %d\n", uptime() - uptime0);

  exec("shutdown", argv_shutdown);
  exit();
}
