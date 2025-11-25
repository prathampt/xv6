#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

  int
newfunc(void)
{
  printf(1, "I am new function and if you are seeing this then the clone is working!!! :)\n");
  exit();
  return 0;
}

  int
main(int argc, char *argv[])
{
  clone(newfunc);
  printf(1, "I the old function: I should be printed only once !!! :)\n");
  exit();
}
