#include "types.h"
#include "stat.h"
#include "user.h"

char *argvnew[] = { "testipc", 0 };

int
main(int argc, char *argv[])
{
  int i = 0;
  while(1) {
    if(fork()) {
      wait();
      printf(1, "%d\n", i++);
    }
    else {
      exec("testipc", argvnew);
    }
  }
  exit();
}
