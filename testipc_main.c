#include "types.h"
#include "stat.h"
#include "user.h"

char *argvnew[] = { "testipc", 0 };

int
main(int argc, char *argv[])
{
  while(1) {
    if(fork()) {
      wait();
    }
    else {
      exec("testipc", argvnew);
    }
  }
  exit();
}
