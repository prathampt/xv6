#include "types.h"
#include "stat.h"
#include "user.h"

// Idle process
int
main(int argc, char *argv[])
{
  while(1)
    doyield();
  exit();
}
