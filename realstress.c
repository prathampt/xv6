#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"

// How many threads are to call each test?
#define NTHREADS  4

char buf[8192];
char *shutdown_argv[] = { "shutdown", 0 };

//
// Module 1
//

// four processes create and delete different files in same directory
int
createdelete(int seed)
{
  printf(1, "%d\n", seed);
  char first = 'a' + seed % 26;
  enum { N = 20 };
  int pid, i, fd, pi;
  char name[32];
  name[0] = first;

  for(pi = 0; pi < 4; pi++){
    pid = fork();
    if(pid < 0){
      printf(1, "createdelete: fork failed\n");
      return 1;
    }

    if(pid == 0){
      name[1] = first + pi;
      name[3] = '\0';
      for(i = 0; i < N; i++){
        name[2] = '0' + i;
        fd = open(name, O_CREATE | O_RDWR);
        if(fd < 0){
          printf(1, "createdelete: create failed\n");
          return 1;
        }
        close(fd);
        if(i > 0 && (i % 2 ) == 0){
          name[2] = '0' + (i / 2);
          if(unlink(name) < 0){
            printf(1, "createdelete: unlink failed\n");
            return 1;
          }
        }
      }
      exit();
    }
  }

  for(pi = 0; pi < 4; pi++){
    wait();
  }

  name[1] = name[2] = name[3] = 0;
  for(i = 0; i < N; i++){
    for(pi = 0; pi < 4; pi++){
      name[1] = first + pi;
      name[2] = '0' + i;
      fd = open(name, 0);
      if((i == 0 || i >= N/2) && fd < 0){
        printf(1, "oops createdelete %s didn't exist\n", name);
        return 1;
      } else if((i >= 1 && i < N/2) && fd >= 0){
        printf(1, "oops createdelete %s did exist\n", name);
        return 1;
      }
      if(fd >= 0) {
        close(fd);
        unlink(name);
      }
    }
  }

  for(i = 0; i < N; i++){
    for(pi = 0; pi < 4; pi++){
      name[1] = first + i;
      name[2] = '0' + i;
      unlink(name);
    }
  }

  return 0;
}

// another concurrent link/unlink/create test,
// to look for deadlocks.
int
linkunlink(int seed)
{
  printf(1, "%d\n", seed);
  int pid, i;
  char *name = "linkunlink";

  pid = fork();
  if(pid < 0){
    printf(1, "linkunlink: fork failed\n");
    return 1;
  }

  unsigned int x = (pid ? 1 : uptime() % 97);
  for(i = 0; i < 100; i++){
    x = x * 1103515245 + 12345;
    if((x % 3) == 0){
      close(open(name, O_RDWR | O_CREATE));
    } else if((x % 3) == 1){
      link("cat", name);
    } else {
      unlink(name);
    }
  }

  if(pid)
    wait();
  else
    exit();

  unlink(name);
  return 0;
}

int
argptest(int seed)
{
  printf(1, "%d\n", seed);
  int fd;
  fd = open("init", O_RDONLY);
  if (fd < 0) {
    printf(2, "argtest: open failed\n");
    return 1;
  }
  read(fd, sbrk(0) - 1, -1);
  close(fd);
  return 0;
}

unsigned long randstate = 1;
unsigned int
rand()
{
  randstate = randstate * 1664525 + 1013904223;
  return randstate;
}

//
// Module 2
//

// two processes write to the same file descriptor
// is the offset shared? does inode locking work?
int
sharedfd(int fd, int seed)
{
  printf(1, "%d\n", seed);
  int pid, i, n, nc, np;
  char buf[10];

  pid = fork();
  char myc = 'a' + seed % 26;
  char myp = 'm' + seed % 26;
  int rwsize = 500;

  memset(buf, pid==0?myc:myp, sizeof(buf));
  for(i = 0; i < rwsize; i++){
    if(write(fd, buf, sizeof(buf)) != sizeof(buf)){
      printf(1, "fstests: write sharedfd failed\n");
      break;
    }
  }
  if(pid == 0)
    exit();
  else
    wait();
  close(fd);
  fd = open("sharedfd", 0);
  if(fd < 0){
    printf(1, "fstests: cannot open sharedfd for reading\n");
    return;
  }
  nc = np = 0;
  while((n = read(fd, buf, sizeof(buf))) > 0){
    for(i = 0; i < sizeof(buf); i++){
      if(buf[i] == myc)
        nc++;
      if(buf[i] == myp)
        np++;
    }
  }
  close(fd);
  if((nc == (rwsize*sizeof(buf))) && (np == (rwsize*sizeof(buf)))){
    return 0;
  } else {
    printf(1, "sharedfd oops %c: %d %c: %d\n", myc, nc, myp, np);
    return 1;
  }
  return 1;
}

int
sharedfd2(int fd, int seed)
{
  printf(1, "%d\n", seed);
  int pid, i, n, nc, np;
  char buf[10];

  pid = fork();
  char myc = 'e' + seed % 26;
  char myp = 'q' + seed % 26;
  int rwsize = 350;

  memset(buf, pid==0?myc:myp, sizeof(buf));
  for(i = 0; i < rwsize; i++){
    if(write(fd, buf, sizeof(buf)) != sizeof(buf)){
      printf(1, "fstests: write sharedfd failed\n");
      break;
    }
  }
  if(pid == 0)
    exit();
  else
    wait();
  close(fd);

  pid = fork();
  fd = open("sharedfd", 0);
  if(fd < 0){
    printf(1, "fstests: cannot open sharedfd for reading\n");
    return 1;
  }

  int wfd;
  if(pid == 0) {
    wfd = open("sharedfdc", O_CREATE | O_RDWR);
  }
  else {
    wfd = open("sharedfdp", O_CREATE | O_RDWR);
  }
  if(wfd < 0){
    printf(1, "fstests: cannot open sharedfdx for reading\n");
    return 1;
  }

  while((n = read(fd, buf, sizeof(buf))) > 0){
    write(wfd, buf, sizeof(buf));
  }

  if(pid == 0)
    exit();
  else
    wait();
  close(fd);

  int fdc = open("sharedfdc", O_RDONLY);
  int fdp = open("sharedfdp", O_RDONLY);
  int n1, n2;
  char buf1[100], buf2[100];

  // diff
  while(1) {
    n1 = read(fdc, buf1, sizeof(buf1));
    n2 = read(fdp, buf2, sizeof(buf2));
    if((n1 == 0) && (n2 == 0)) {
      close(fdc);
      close(fdp);
      return 0;
    }
    if(n1 != n2) {
      printf(1, "sharedfd2: diff failed: n1=%d, n2=%d\n", n1, n2);
      return 1;
    }
    for(i = 0; i < n1; i++) {
      if(buf1[i] != buf2[i]) {
        printf(1, "sharedfd2: diff failed\n");
        return 1;
      }
    }
  }
}

// four processes write different files at the same
// time, to test block allocation.
int
fourfiles(int seed)
{
  printf(1, "%d\n", seed);
  int fd, pid, i, j, n, total, pi;
  char names[4][4];
  char *fname;

  for(i = 0; i < 4; i++) {
    names[i][0] = 'f' + seed % 26;
    names[i][1] = 'a' + seed % 26;
    names[i][2] = '0' + i;
    names[i][3] = 0;
  }

  for(pi = 0; pi < 4; pi++){
    fname = names[pi];
    unlink(fname);

    pid = fork();
    if(pid < 0){
      printf(1, "fourfiles: fork failed\n");
      return 1;
    }

    if(pid == 0){
      fd = open(fname, O_CREATE | O_RDWR);
      if(fd < 0){
        printf(1, "fourfiles: create failed\n");
        return 1;
      }

      memset(buf, '0'+pi, 512);
      for(i = 0; i < 12; i++){
        if((n = write(fd, buf, 500)) != 500){
          printf(1, "fourfiles: write failed %d\n", n);
          return 1;
        }
      }
      exit();
    }
  }

  for(pi = 0; pi < 4; pi++){
    wait();
  }

  for(i = 0; i < 4; i++){
    fname = names[i];
    fd = open(fname, 0);
    total = 0;
    while((n = read(fd, buf, sizeof(buf))) > 0){
      for(j = 0; j < n; j++){
        if(buf[j] != '0'+i){
          printf(1, "fourfiles: wrong char\n");
          return 1;
        }
      }
      total += n;
    }
    close(fd);
    if(total != 12*500){
      printf(1, "fourfiles: wrong length %d\n", total);
      return 1;
    }
    unlink(fname);
  }
  return 0;
}

// test concurrent create/link/unlink of the same file
int
concreate(int seed)
{
  printf(1, "%d\n", seed);
  char file[3];
  int i, pid, n, fd;
  char fa[40];
  struct {
    ushort inum;
    char name[14];
  } de;

  int newpid = fork();
  char ch = 'C' + seed % 26;
  char fi[3];
  fi[0] = ch;
  fi[1] = '0';
  fi[2] = 0;

  file[0] = ch;
  file[2] = '\0';
  for(i = 0; i < 40; i++){
    file[1] = '0' + i;
    unlink(file);
    pid = fork();
    if(pid && (i % 3) == 1){
      link(fi, file);
    } else if(pid == 0 && (i % 5) == 1){
      link(fi, file);
    } else {
      fd = open(file, O_CREATE | O_RDWR);
      if(fd < 0){
        printf(1, "concreate: create %s failed\n", file);
        return 1;
      }
      close(fd);
    }
    if(pid == 0)
      exit();
    else
      wait();
  }

  if(newpid > 0)
    wait();
  else
    exit();

  memset(fa, 0, sizeof(fa));
  fd = open(".", 0);
  n = 0;
  while(read(fd, &de, sizeof(de)) > 0){
    if(de.inum == 0)
      continue;
    if(de.name[0] == ch && de.name[2] == '\0'){
      i = de.name[1] - '0';
      if(i < 0 || i >= sizeof(fa)){
        printf(1, "concreate: weird file %s\n", de.name);
        return 1;
      }
      if(fa[i]){
        printf(1, "concreate: duplicate file %s\n", de.name);
        return 1;
      }
      fa[i] = 1;
      n++;
    }
  }
  close(fd);

  if(n != 40){
    printf(1, "concreate: not enough files in directory listing: file: %s ch: %c\n", file, ch);
    return 1;
  }

  for(i = 0; i < 40; i++){
    file[1] = '0' + i;
    pid = fork();
    if(pid < 0){
      printf(1, "concreate: fork failed\n");
      exit();
    }
    if(((i % 3) == 0 && pid == 0) ||
       ((i % 3) == 1 && pid != 0)){
      close(open(file, 0));
      close(open(file, 0));
      close(open(file, 0));
      close(open(file, 0));
    } else {
      unlink(file);
      unlink(file);
      unlink(file);
      unlink(file);
    }
    if(pid == 0)
      exit();
    else
      wait();
  }
  return 0;
}

int (*module_1[])(int) = {
  [0] argptest,
  [1] createdelete,
  [2] linkunlink,
};

int (*module_2[])(int, int) = {
  [0] sharedfd,
  [1] sharedfd2,
};

int (*module_3[])(int) = {
  [0] concreate,
  [1] fourfiles,
};

void
module1(void)
{
  int uptime0 = uptime();
  printf(1, "module 1 started\n");
  int ret, i;
  for(i = 0; i < NELEM(module_1)*NTHREADS; i++) {
    if(fork() == 0) {
      ret = module_1[i/NTHREADS](i%NTHREADS);
      if(ret) {
        dopanic("TEST FAILED\n");
      }
      exit();
    }
  }
  // wait for all children
  for(i = 0; i < NELEM(module_1)*NTHREADS; i++) {
    wait();
  }
  printf(1, "module 1 passed\n");
  printf(1, "module 1 time: %d\n", uptime() - uptime0);
}

void
module2(void)
{
  int uptime0 = uptime();
  printf(1, "module 2 started\n");
  int ret, i;
  int fd = open("sharedfd", O_CREATE | O_RDWR);
  if(fd < 0){
    dopanic("fstests: cannot open sharedfd for writing");
  }
  for(i = 0; i < NELEM(module_2)*NTHREADS; i++) {
    if(fork() == 0) {
      ret = module_2[i/NTHREADS](fd, i%NTHREADS);
      if(ret) {
        dopanic("TEST FAILED\n");
      }
      exit();
    }
  }
  // wait for all children
  for(i = 0; i < NELEM(module_1)*NTHREADS; i++) {
    wait();
  }
  close(fd);
  unlink("sharedfd");
  unlink("sharedfdc");
  unlink("sharedfdp");
  printf(1, "module 2 passed\n");
  printf(1, "module 2 time: %d\n", uptime() - uptime0);
}

void
module3(void)
{
  int uptime0 = uptime();
  printf(1, "module 3 started\n");
  int ret, i;
  for(i = 0; i < NELEM(module_3)*NTHREADS; i++) {
    if(fork() == 0) {
      ret = module_3[i/NTHREADS](i%NTHREADS);
      if(ret) {
        dopanic("TEST FAILED\n");
      }
      exit();
    }
  }
  // wait for all children
  for(i = 0; i < NELEM(module_3)*NTHREADS; i++) {
    wait();
  }
  printf(1, "module 3 passed\n");
  printf(1, "module 3 time: %d\n", uptime() - uptime0);
}

int
main(int argc, char *argv[])
{
  int i;
  if(NTHREADS < 1 || NTHREADS > 4) {
    printf(1, "invalid number of threads, 1 <= NTHREADS <= 4\n");
    exit();
  }

  printf(1, "usertests starting\n");

  if(open("usertests.ran", 0) >= 0){
    printf(1, "already ran user tests -- rebuild fs.img\n");
    exit();
  }
  close(open("usertests.ran", O_CREATE));

  int uptime0 = uptime();

  module1();
  module2();
  module3();
  printf(1, "Total time: %d\n", uptime() - uptime0);

  exec("shutdown", shutdown_argv);
  exit();
}
