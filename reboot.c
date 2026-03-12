#include "types.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  printf(1, "bye bye...\n");
  poweroff();
  // we should not return here
  exit();
}
