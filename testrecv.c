#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  char path[64];
  int flags;
  int vecnum;
  int index;
  int result = recv("ddsd", &vecnum, &index, &path, &flags);
  printf(1, "printing stuff from obtained from recv(), recv() returned %d\n", result);
  printf(1, "vecnum: %d\n", vecnum);
  printf(1, "index: %d\n", index);
  printf(1, "path: %s\n", path);
  printf(1, "flags: %d\n", flags);
  exit();
}
