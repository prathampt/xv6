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

//
// Module 3
//

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

// try to find any races between exit and wait
int
exitwait(int seed)
{
  int i, pid;
  printf(1, "%d\n", seed);

  for(i = 0; i < 100; i++){
    pid = fork();
    if(pid < 0){
      printf(1, "fork failed\n");
      return 1;
    }
    if(pid){
      if(wait() != pid){
        printf(1, "wait wrong pid\n");
        return 1;
      }
    } else {
      exit();
    }
  }
  return 0;
}

//
// Module 4
//

// does exec return an error if the arguments
// are larger than a page? or does it write
// below the stack and wreck the instructions/data?
int
bigargtest(int seed)
{
  int pid, fd;
  char okfile[16];

  // This file is opened just to allow the parent to check
  // whether exec() failed in the child
  strcpy(okfile, "bigarg-ok");
  okfile[strlen(okfile)] = 'a' + seed % 26;
  okfile[strlen(okfile)] = 0;

  unlink(okfile);
  pid = fork();
  if(pid == 0){
    static char *args[MAXARG];
    int i;
    for(i = 0; i < MAXARG-1; i++)
      args[i] = "bigargs test: failed\n                                                                                                                                                                                                       ";
    args[MAXARG-1] = 0;
    exec("echo", args);
    fd = open(okfile, O_CREATE);
    close(fd);
    exit();
  } else if(pid < 0){
    printf(1, "bigargtest: fork failed\n");
    return 1;
  }
  wait();
  fd = open(okfile, 0);
  if(fd < 0){
    printf(1, "bigargtest: open failed!\n");
    return 1;
  }
  close(fd);
  unlink(okfile);
  return 0;
}

// test writes that are larger than the log.
int
bigwrite(int seed)
{
  printf(1, "%d\n", seed);
  int fd, sz;
  // added more blocks and another child for increased load
  int nblocks = 32;
  int pid = fork();

  unlink("bigwrite");
  for(sz = 499; sz < nblocks*512; sz += 471){
    fd = open("bigwrite", O_CREATE | O_RDWR);
    if(fd < 0){
      printf(1, "bigwrite: cannot create bigwrite\n");
      return 1;
    }
    int i;
    for(i = 0; i < 2; i++){
      int cc = write(fd, buf, sz);
      if(cc != sz){
        printf(1, "bigwrite: write(%d) ret %d\n", sz, cc);
        return 1;
      }
    }
    close(fd);
  }
  if(pid == 0)
    exit();
  else
    wait();

  return 0;
}

// does unintialized data start out zero?
char uninit[8749];
int
bsstest(int seed)
{
  printf(1, "%d\n", seed);
  int i;
  for(i = 0; i < sizeof(uninit); i++){
    if(uninit[i] != '\0'){
      printf(1, "bsstest: failed\n");
      return 1;
    }
  }
  return 0;
}

int
sbrktest(int seed)
{
  printf(1, "%d\n", seed);
  int fds[2], pid, pids[10], ppid;
  char *a, *b, *c, *lastaddr, *oldbrk, *p, scratch;
  uint amt;

  oldbrk = sbrk(0);

  // can one sbrk() less than a page?
  a = sbrk(0);
  int i;
  for(i = 0; i < 5000; i++){
    b = sbrk(1);
    if(b != a){
      printf(1, "sbrktest: failed %d %x %x\n", i, a, b);
      return 1;
    }
    *b = 1;
    a = b + 1;
  }
  pid = fork();
  if(pid < 0){
    printf(1, "sbrktest: fork failed\n");
    return 1;
  }
  c = sbrk(1);
  c = sbrk(1);
  if(c != a + 1){
    printf(1, "sbrktest: failed post-fork\n");
    return 1;
  }
  if(pid == 0)
    exit();
  wait();

  // can one grow address space to something big?
#define BIG ((PHYSTOP/4) - 6)
  a = sbrk(0);
  amt = (BIG) - (uint)a;
  p = sbrk(amt);
  if (p != a) {
    printf(1, "sbrktest: failed to grow big address space; enough phys mem?\n");
    return 1;
  }
  lastaddr = (char*) (BIG-1);
  *lastaddr = 99;

  // can one de-allocate?
  a = sbrk(0);
  c = sbrk(-4096);
  if(c == (char*)0xffffffff){
    printf(1, "sbrktest: could not deallocate\n");
    return 1;
  }
  c = sbrk(0);
  if(c != a - 4096){
    printf(1, "sbrktest: deallocation produced wrong address, a %x c %x\n", a, c);
    return 1;
  }

  // can one re-allocate that page?
  a = sbrk(0);
  c = sbrk(4096);
  if(c != a || sbrk(0) != a + 4096){
    printf(1, "sbrktest: re-allocation failed, a %x c %x\n", a, c);
    return 1;
  }
  if(*lastaddr == 99){
    // should be zero
    printf(1, "sbrktest: de-allocation didn't really deallocate\n");
    return 1;
  }

  a = sbrk(0);
  c = sbrk(-(sbrk(0) - oldbrk));
  if(c != a){
    printf(1, "sbrktest: downsize failed, a %x c %x\n", a, c);
    return 1;
  }

  // can we read the kernel's memory?
  for(a = (char*)(KERNBASE); a < (char*) (KERNBASE+2000000); a += 50000){
    ppid = getpid();
    pid = fork();
    if(pid < 0){
      printf(1, "sbrktest: fork failed\n");
      return 1;
    }
    if(pid == 0){
      printf(1, "sbrktest: oops could read %x = %x\n", a, *a);
      kill(ppid);
      return 1;
    }
    wait();
  }

  // if we run the system out of memory, does it clean up the last
  // failed allocation?
  if(pipe(fds) != 0){
    printf(1, "sbrktest: pipe() failed\n");
    return 1;
  }
  for(i = 0; i < sizeof(pids)/sizeof(pids[0]); i++){
    if((pids[i] = fork()) == 0){
      // allocate a lot of memory
      sbrk(BIG - (uint)sbrk(0));
      write(fds[1], "x", 1);
      // sit around until killed
      for(;;) sleep(1000);
    }
    if(pids[i] != -1)
      read(fds[0], &scratch, 1);
  }
  // if those failed allocations freed up the pages they did allocate,
  // we'll be able to allocate here
  c = sbrk(4096);
  for(i = 0; i < sizeof(pids)/sizeof(pids[0]); i++){
    if(pids[i] == -1)
      continue;
    kill(pids[i]);
    wait();
  }
  if(c == (char*)0xffffffff){
    printf(1, "sbrktest: failed sbrk leaked memory\n");
    return 1;
  }

  if(sbrk(0) > oldbrk)
    sbrk(-(sbrk(0) - oldbrk));

  return 0;
}

void
validateint(int *p)
{
  int res;
  asm("mov %%esp, %%ebx\n\t"
      "mov %3, %%esp\n\t"
      "int %2\n\t"
      "mov %%ebx, %%esp" :
      "=a" (res) :
      "a" (SYS_sleep), "n" (T_SYSCALL), "c" (p) :
      "ebx");
}

int
validatetest(int seed)
{
  printf(1, "%d\n", seed);
  int hi, pid;
  uint p;

  hi = 8192*1024; // increased for more load

  for(p = 0; p <= (uint)hi; p += 734){
    if((pid = fork()) == 0){
      // try to crash the kernel by passing in a badly placed integer
      validateint((int*)p);
      exit();
    }
    sleep(0);
    sleep(0);
    kill(pid);
    wait();

    // try to crash the kernel by passing in a bad string pointer
    if(link("nosuchfile", (char*)p) != -1){
      printf(1, "link should not succeed\n");
      return 1;
    }
  }
  return 0;
}

int (*module_4[])(int) = {
  [0] bigargtest,
  [1] bigwrite,
  [2] bsstest,
  [3] sbrktest,
  [4] validatetest,
};

void
module4(void)
{
  int uptime0 = uptime();
  printf(1, "module 4 started\n");
  int ret, i;
  for(i = 0; i < NELEM(module_4)*NTHREADS; i++) {
    if(fork() == 0) {
      ret = module_4[i/NTHREADS](i%NTHREADS);
      if(ret) {
        dopanic("TEST FAILED\n");
      }
      exit();
    }
  }
  // wait for all children
  for(i = 0; i < NELEM(module_4)*NTHREADS; i++) {
    wait();
  }
  unlink("bigwrite");  // bigwrite

  printf(1, "module 4 passed\n");
  printf(1, "module 4 time: %d\n", uptime() - uptime0);
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
  [2] exitwait,
};

//
// Module 5
//

int
opentest(int seed)
{
  int fd;
  printf(1, "%d\n", seed);
  fd = open("echo", 0);
  if(fd < 0){
    printf(1, "open echo failed!\n");
    return 1;
  }
  close(fd);
  fd = open("doesnotexist", 0);
  if(fd >= 0){
    printf(1, "open doesnotexist succeeded!\n");
    return 1;
  }
  return 0;
}

int
writetest(int seed)
{
  int fd;
  int i;
  printf(1, "%d\n", seed);

  fd = open("small", O_CREATE|O_RDWR);
  if(fd < 0){
    printf(1, "error: creat small failed!\n");
    return 1;
  }
  for(i = 0; i < 100; i++){
    if(write(fd, "aaaaaaaaaa", 10) != 10){
      printf(1, "error: write aa %d new file failed\n", i);
      return 1;
    }
    if(write(fd, "bbbbbbbbbb", 10) != 10){
      printf(1, "error: write bb %d new file failed\n", i);
      return 1;
    }
  }
  close(fd);
  fd = open("small", O_RDONLY);
  if(fd < 0){
    printf(1, "error: open small failed!\n");
    return 1;
  }
  i = read(fd, buf, 2000);
  if(i != 2000){
    printf(1, "read failed\n");
    return 1;
  }
  close(fd);

  return 0;
}

char buf[8192];

int
writetest1(int seed)
{
  int i, fd, n;
  printf(1, "%d\n", seed);
  char filename[5];
  filename[0] = 'b';
  filename[1] = 'i';
  filename[2] = 'g';
  filename[3] = 'w' + seed % 26;
  filename[4] = 0;

  fd = open(filename, O_CREATE|O_RDWR);
  if(fd < 0){
    printf(1, "error: creat big failed!\n");
    return 1;
  }

  for(i = 0; i < MAXFILE; i++){
    ((int*)buf)[0] = i;
    if(write(fd, buf, 512) != 512){
      printf(1, "error: write big file failed\n", i);
      return 1;
    }
  }

  close(fd);

  fd = open(filename, O_RDONLY);
  if(fd < 0){
    printf(1, "error: open big failed!\n");
    return 1;
  }

  n = 0;
  for(;;){
    i = read(fd, buf, 512);
    if(i == 0){
      if(n == MAXFILE - 1){
        printf(1, "read only %d blocks from big", n);
        return 1;
      }
      break;
    } else if(i != 512){
      printf(1, "read failed %d\n", i);
      return 1;
    }
    if(((int*)buf)[0] != n){
      printf(1, "read content of block %d is %d\n",
             n, ((int*)buf)[0]);
      return 1;
    }
    n++;
  }
  close(fd);
  if(unlink(filename) < 0){
    printf(1, "unlink big failed\n");
    return 1;
  }
  return 0;
}

int
createtest(int seed)
{
  int i, fd;
  printf(1, "%d\n", seed);
  char name[4];

  name[0] = 'a' + seed % 26;
  name[2] = 'c';
  name[3] = 0;
  for(i = 0; i < 40; i++){
    name[1] = '0' + i;
    fd = open(name, O_CREATE|O_RDWR);
    close(fd);
  }
  name[0] = 'a' + seed % 26;
  name[2] = 'c';
  name[3] = 0;
  for(i = 0; i < 40; i++){
    name[1] = '0' + i;
    unlink(name);
  }
  return 0;
}

int
openiputtest(void)
{
  int pid;

  if(mkdir("oidir") < 0){
    printf(1, "mkdir oidir failed\n");
    return 1;
  }
  pid = fork();
  if(pid < 0){
    printf(1, "fork failed\n");
    return 1;
  }
  if(pid == 0){
    int fd = open("oidir", O_RDWR);
    if(fd >= 0){
      printf(1, "open directory for write succeeded\n");
      return 1;
    }
    exit();
  }
  sleep(1);
  if(unlink("oidir") != 0){
    printf(1, "unlink failed\n");
    return 1;
  }
  wait();
  return 0;
}

// does exit() call iput(p->cwd) in a transaction?
int
exitiputtest(void)
{
  int pid;

  pid = fork();
  if(pid < 0){
    printf(1, "fork failed\n");
    return 1;
  }
  if(pid == 0){
    if(mkdir("eiputdir") < 0){
      printf(1, "mkdir failed\n");
      return 1;
    }
    if(chdir("eiputdir") < 0){
      printf(1, "child chdir failed\n");
      return 1;
    }
    if(unlink("../eiputdir") < 0){
      printf(1, "unlink ../eiputdir failed\n");
      return 1;
    }
    exit();
  }
  wait();
  return 0;
}

// does chdir() call iput(p->cwd) in a transaction?
int
iputtest(void)
{
  if(mkdir("iputdir") < 0){
    printf(1, "mkdir failed\n");
    return 1;
  }
  if(chdir("iputdir") < 0){
    printf(1, "chdir iputdir failed\n");
    return 1;
  }
  if(unlink("../iputdir") < 0){
    printf(1, "unlink ../iputdir failed\n");
    return 1;
  }
  if(chdir("/") < 0){
    printf(1, "chdir / failed\n");
    return 1;
  }
  return 0;
}

//
// Module 6
//

int
mem(int seed)
{
  void *m1, *m2;
  int pid, ppid;

  printf(1, "%d\n", seed);
  ppid = getpid();
  if((pid = fork()) == 0){
    m1 = 0;
    while((m2 = malloc(10001)) != 0){
      *(char**)m2 = m1;
      m1 = m2;
    }
    while(m1){
      m2 = *(char**)m1;
      free(m1);
      m1 = m2;
    }
    m1 = malloc(1024*5);
    if(m1 == 0){
      printf(1, "couldn't allocate mem?!!\n");
      kill(ppid);
      return 1;
    }
    free(m1);
    exit();
  } else {
    wait();
  }
  return 0;
}

// simple fork and pipe read/write

int
pipe1(int seed)
{
  int fds[2], pid;
  int seq, i = 1, n, cc, total;

  printf(1, "%d\n", seed);
  while(pipe(fds) != 0) {
    printf(1, "pipe fail count: %d seed: %d\n", i++, seed);
  }
  pid = fork();
  seq = 0;
  if(pid == 0){
    close(fds[0]);
    for(n = 0; n < 5; n++){
      for(i = 0; i < 1033; i++)
        buf[i] = seq++;
      if(write(fds[1], buf, 1033) != 1033){
        printf(1, "pipe1 oops 1\n");
        return 1;
      }
    }
    exit();
  } else if(pid > 0){
    close(fds[1]);
    total = 0;
    cc = 1;
    while((n = read(fds[0], buf, cc)) > 0){
      for(i = 0; i < n; i++){
        if((buf[i] & 0xff) != (seq++ & 0xff)){
          printf(1, "pipe1 oops 2\n");
          return 1;
        }
      }
      total += n;
      cc = cc * 2;
      if(cc > sizeof(buf))
        cc = sizeof(buf);
    }
    if(total != 5 * 1033){
      printf(1, "pipe1 oops 3 total %d\n", total);
      return 1;
    }
    close(fds[0]);
    wait();
  } else {
    printf(1, "fork() failed\n");
    return 1;
  }
  return 0;
}

int
preempt(int seed)
{
  int pid1, pid2, pid3, i = 1;
  int pid11, pid22, pid33;
  int pfds[2], pfds2[2];

  printf(1, "%d\n", seed);
  pid1 = fork();
  if(pid1 == 0)
    for(;;)
      ;

  pid11 = fork();
  if(pid11 == 0)
    for(;;)
      ;

  pid2 = fork();
  if(pid2 == 0)
    for(;;)
      ;

  pid22 = fork();
  if(pid22 == 0)
    for(;;)
      ;

  while(pipe(pfds) != 0) {
    printf(1, "pipe fail count: %d seed: %d\n", i++, seed);
  }
  pid3 = fork();
  if(pid3 == 0){
    close(pfds[0]);
    if(write(pfds[1], "x", 1) != 1)
      printf(1, "preempt write error");
    close(pfds[1]);
    for(;;)
      ;
  }

  while(pipe(pfds2) != 0) {
    printf(1, "pipe fail count: %d seed: %d\n", i++, seed);
  }
  pid33 = fork();
  if(pid33 == 0){
    close(pfds2[0]);
    if(write(pfds2[1], "x", 1) != 1)
      printf(1, "preempt write error");
    close(pfds2[1]);
    for(;;)
      ;
  }

  close(pfds[1]);
  while(read(pfds[0], buf, sizeof(buf)) != 1)
    ;
  close(pfds[0]);

  close(pfds2[1]);
  while(read(pfds2[0], buf, sizeof(buf)) != 1)
    ;
  close(pfds2[0]);

  kill(pid1);
  kill(pid2);
  kill(pid3);
  kill(pid11);
  kill(pid22);
  kill(pid33);
  wait();
  wait();
  wait();
  wait();
  wait();
  wait();
  return 0;
}


// test that fork fails gracefully
// the forktest binary also does this, but it runs out of proc entries first.
// inside the bigger usertests binary, we run out of memory first.
int
forktest(int seed)
{
  int n, pid;

  printf(1, "%d\n", seed);
  for(n=0; n<1000; n++){
    pid = fork();
    if(pid < 0)
      break;
    if(pid == 0)
      exit();
  }

  if(n == 1000){
    printf(1, "fork claimed to work 1000 times!\n");
    return 1;
  }

  for(; n > 0; n--){
    if(wait() < 0){
      printf(1, "wait stopped early\n");
      return 1;
    }
  }

  if(wait() != -1){
    printf(1, "wait got too many\n");
    return 1;
  }

  return 0;
}

int
uio(int seed)
{
  #define RTC_ADDR 0x70
  #define RTC_DATA 0x71

  ushort port = 0;
  uchar val = 0;
  int pid;

  printf(1, "%d\n", seed);
  pid = fork();
  if(pid == 0){
    port = RTC_ADDR;
    val = 0x09;  /* year */
    /* http://wiki.osdev.org/Inline_Assembly/Examples */
    asm volatile("outb %0,%1"::"a"(val), "d" (port));
    port = RTC_DATA;
    asm volatile("inb %1,%0" : "=a" (val) : "d" (port));
    printf(1, "uio: uio succeeded; test FAILED\n");
    return 1;
  } else if(pid < 0){
    printf (1, "fork failed\n");
    return 1;
  }
  wait();
  return 0;
}

int (*module_5[])(int) = {
  [0] opentest,
  [1] writetest,
  [2] writetest1,
  [3] createtest,
  [4] mem,
};

int (*module_6[])(int) = {
  [0] pipe1,
  [1] preempt,
  [2] uio,
  [3] forktest,
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

void
module5(void)
{
  int uptime0 = uptime();
  printf(1, "module 5 started\n");
  int ret, i;
  for(i = 0; i < NELEM(module_5)*NTHREADS; i++) {
    if(fork() == 0) {
      ret = module_5[i/NTHREADS](i%NTHREADS);
      if(ret) {
        dopanic("TEST FAILED\n");
      }
      exit();
    }
  }
  // wait for all children
  for(i = 0; i < NELEM(module_5)*NTHREADS; i++) {
    wait();
  }
  unlink("small");

  for(i = 0; i < 3; i++) {
    if(fork() == 0) {
      switch(i) {
        case 0:
          if(openiputtest()) {
            dopanic("TEST FAILED\n");
          }
          exit();
          break;
        case 1:
          if(exitiputtest()) {
            dopanic("TEST FAILED\n");
          }
          exit();
          break;
        case 2:
          if(iputtest()) {
            dopanic("TEST FAILED\n");
          }
          exit();
          break;
        default:
          break;
      }
    }
  }
  for(i = 0; i < 3; i++) {
    wait();
  }
  printf(1, "module 5 passed\n");
  printf(1, "module 5 time: %d\n", uptime() - uptime0);
}

void
module6(void)
{
  int uptime0 = uptime();
  printf(1, "module 6 started\n");
  int ret, i;
  for(i = 0; i < NELEM(module_6)*NTHREADS; i++) {
    if(fork() == 0) {
      ret = module_6[i/NTHREADS](i%NTHREADS);
      if(ret) {
        dopanic("TEST FAILED\n");
      }
      exit();
    }
  }
  // wait for all children
  for(i = 0; i < NELEM(module_6)*NTHREADS; i++) {
    wait();
  }
  printf(1, "module 6 passed\n");
  printf(1, "module 6 time: %d\n", uptime() - uptime0);
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

  /*
  module1();
  module2();
  module3();
  module4();
  module5();
  */
  module6();
  printf(1, "Total time: %d\n", uptime() - uptime0);

  exec("shutdown", shutdown_argv);
  exit();
}
