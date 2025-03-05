#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

char buf[512];

void
cp(int fdin, int fdout)
{
  int n;

  while((n = read(fdin, buf, sizeof(buf))) > 0) {
    if (write(fdout, buf, n) != n) {
      printf(1, "cp: write error\n");
      exit();
    }
  }
  if(n < 0){
    printf(1, "cp: read error\n");
    exit();
  }
}

int
main(int argc, char *argv[])
{
  int fdin, fdout, i;

  if(argc != 3){
    printf(1, "cp: invalid usuage");
    exit();
  }

  if((fdin = open(argv[1], O_RDONLY)) < 0){
      printf(1, "cp: cannot open %s\n", argv[1]);
      exit();
  }
  
  if((fdout = open(argv[2], O_CREATE | O_WRONLY)) < 0){
      printf(1, "cp: cannot open %s\n", argv[2]);
      exit();
  }
  
  cp(fdin, fdout);
  exit();
}
