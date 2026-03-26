#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"

char buf[8192];
int nthreads = 4;

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
  while((pid = fork()) < 0)
    ;
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

  while((pid = fork()) < 0)
    ;

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
      exit();
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
  printf(1, "%d\n", seed);
  int pid1, pid2, pid3;
  int pfds[2];

  pid1 = fork();
  if(pid1 == 0)
    for(;;)
      ;

  pid2 = fork();
  if(pid2 == 0)
    for(;;)
      ;

  pipe(pfds);
  pid3 = fork();
  if(pid3 == 0){
    close(pfds[0]);
    if(write(pfds[1], "x", 1) != 1)
      printf(1, "preempt write error");
    close(pfds[1]);
    for(;;)
      ;
  }

  close(pfds[1]);
  if(read(pfds[0], buf, sizeof(buf)) != 1){
    printf(1, "preempt read error");
    return 1;
  }
  close(pfds[0]);
  kill(pid1);
  kill(pid2);
  kill(pid3);
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
    printf (1, "uio: fork failed\n");
    return 1;
  }
  wait();
  return 0;
}

//
// Module 7
//

int
rmdot(int seed)
{
  printf(1, "%d\n", seed);

  // "dotsx"
  char dotname[16];
  strcpy(dotname, "dots");
  dotname[strlen(dotname)] = 'a' + seed % 26;

  // "dotsx/."
  char dotname_dot[16];
  strcpy(dotname_dot, dotname);

  dotname_dot[5] = '/';
  dotname_dot[6] = '.';
  dotname_dot[7] = 0;

  char dotname_dotdot[16];
  strcpy(dotname_dotdot, dotname_dot);
  dotname_dotdot[8] = '.';
  dotname_dotdot[9] = 0;

  // mkdir dotsx
  if(mkdir(dotname) != 0){
    printf(1, "rmdot: mkdir %s failed\n", dotname);
    return 1;
  }

  // cd dotsx
  if(chdir(dotname) != 0){
    printf(1, "rmdot: chdir %s failed\n", dotname);
    return 1;
  }

  // mkdir dotsx
  if(mkdir(dotname) != 0){
    printf(1, "rmdot: next mkdir %s failed\n", dotname);
    return 1;
  }

  // cd dotsx
  // cwd becomes /dotsx/dotsx/
  if(chdir(dotname) != 0){
    printf(1, "rmdot: next chdir %s failed\n", dotname);
    return 1;
  }

  // rm .
  if(unlink(".") == 0){
    printf(1, "rmdot: rm . worked!\n");
    return 1;
  }

  // rm ..
  if(unlink("..") == 0){
    printf(1, "rmdot: rm .. worked!\n");
    return 1;
  }

  // cd ..
  // cwd is /dotsx
  if(chdir("..") != 0){
    printf(1, "rmdot: chdir .. failed\n");
    return 1;
  }

  // rm dotsx
  if(unlink(dotname) != 0){
    printf(1, "rmdot: unlink %s failed!\n", dotname);
    return 1;
  }

  // cd /
  if(chdir("/") != 0){
    printf(1, "rmdot: chdir / failed\n");
    return 1;
  }

  // rm dotsx/.
  if(unlink(dotname_dot) == 0){
    printf(1, "rmdot: unlink %s worked!\n", dotname_dot);
    return 1;
  }

  // rm dotsx/..
  if(unlink(dotname_dotdot) == 0){
    printf(1, "rmdot: unlink %s worked!\n", dotname_dotdot);
    return 1;
  }

  // rm dotsx
  if(unlink(dotname) != 0){
    printf(1, "rmdot: unlink %s failed!\n", dotname);
    return 1;
  }
  return 0;
}


int
linktest(int seed)
{
  printf(1, "%d\n", seed);
  int fd;

  char lf1[16], lf2[16];

  strcpy(lf1, "lf1x");
  lf1[3] = 'a' + seed % 26;

  strcpy(lf2, "lf2x");
  lf2[3] = 'a' + seed % 26;

  unlink(lf1);
  unlink(lf2);

  fd = open(lf1, O_CREATE|O_RDWR);
  if(fd < 0){
    printf(1, "create lf1 failed\n");
    return 1;
  }
  if(write(fd, "hello", 5) != 5){
    printf(1, "write lf1 failed\n");
    return 1;
  }
  close(fd);

  if(link(lf1, lf2) < 0){
    printf(1, "link lf1 lf2 failed\n");
    return 1;
  }
  unlink(lf1);

  if(open(lf1, 0) >= 0){
    printf(1, "unlinked lf1 but it is still there!\n");
    return 1;
  }

  fd = open(lf2, 0);
  if(fd < 0){
    printf(1, "open lf2 failed\n");
    return 1;
  }
  if(read(fd, buf, sizeof(buf)) != 5){
    printf(1, "read lf2 failed\n");
    return 1;
  }
  close(fd);

  if(link(lf2, lf2) >= 0){
    printf(1, "link lf2 lf2 succeeded! oops\n");
    return 1;
  }

  unlink(lf2);
  if(link(lf2, lf1) >= 0){
    printf(1, "link non-existant succeeded! oops\n");
    return 1;
  }

  if(link(".", lf1) >= 0){
    printf(1, "link . lf1 succeeded! oops\n");
    return 1;
  }

  return 0;
}

// test that iput() is called at the end of _namei()
int
iref(int seed)
{
  printf(1, "%d\n", seed);
  int i, fd;
  char irefd[16];
  strcpy(irefd, "irefdx");
  irefd[5] = 'a' + seed % 26;

  // the 10 is NINODE/5
  for(i = 0; i < 10 + 1; i++){
    if(mkdir(irefd) != 0){
      printf(1, "irefd: mkdir failed\n");
      return 1;
    }
    if(chdir(irefd) != 0){
      printf(1, "irefd: chdir failed\n");
      return 1;
    }

    mkdir("");
    link("README", "");
    fd = open("", O_CREATE);
    if(fd >= 0)
      close(fd);
    fd = open("xx", O_CREATE);
    if(fd >= 0)
      close(fd);
    unlink("xx");
  }

  chdir("/");
  return 0;
}

// directory that uses indirect blocks
int
bigdir(int seed)
{
  printf(1, "%d\n", seed);
  int i, fd;
  int count = 1000;
  char name[10];
  char bd[4];
  bd[0] = 'p' + seed % 26;
  bd[1] = 'd';
  bd[2] = 0;

  if(mkdir(bd) == -1) {
    printf(1, "bigdir: mkdir failed\n");
    return 1;
  }

  if(chdir(bd) == -1) {
    printf(1, "bigdir: chdir failed\n");
    return 1;
  }

  fd = open(bd, O_CREATE);
  if(fd < 0){
    printf(1, "bigdir create failed\n");
    return 1;
  }
  close(fd);

  for(i = 0; i < count; i++){
    name[0] = 'p' + seed % 26;
    name[1] = '0' + (i / 64);
    name[2] = '0' + (i % 64);
    name[3] = '\0';
    if(link(bd, name) != 0){
      printf(1, "bigdir link failed\n");
      return 1;
    }
  }

  unlink(bd);
  for(i = 0; i < count; i++){
    name[0] = 'p' + seed % 26;
    name[1] = '0' + (i / 64);
    name[2] = '0' + (i % 64);
    name[3] = '\0';
    if(unlink(name) != 0){
      printf(1, "bigdir unlink failed");
      return 1;
    }
  }
  return 0;
}

int
dirfile(int seed)
{
  printf(1, "%d\n", seed);
  int fd;

  fd = open("dirfile", O_CREATE);
  if(fd < 0){
    printf(1, "create dirfile failed\n");
    return 1;
  }
  close(fd);
  if(chdir("dirfile") == 0){
    printf(1, "chdir dirfile succeeded!\n");
    return 1;
  }
  fd = open("dirfile/xx", 0);
  if(fd >= 0){
    printf(1, "create dirfile/xx succeeded!\n");
    return 1;
  }
  fd = open("dirfile/xx", O_CREATE);
  if(fd >= 0){
    printf(1, "create dirfile/xx succeeded!\n");
    return 1;
  }
  if(mkdir("dirfile") == 0){
    printf(1, "mkdir dirfile succeeded!\n");
    return 1;
  }
  if(mkdir("dirfile/xx") == 0){
    printf(1, "mkdir dirfile/xx succeeded!\n");
    return 1;
  }
  if(unlink("dirfile/xx") == 0){
    printf(1, "unlink dirfile/xx succeeded!\n");
    return 1;
  }
  if(link("README", "dirfile/xx") == 0){
    printf(1, "link to dirfile/xx succeeded!\n");
    return 1;
  }

  fd = open(".", O_RDWR);
  if(fd >= 0){
    printf(1, "open . for writing succeeded!\n");
    return 1;
  }
  fd = open("..", O_RDWR);
  if(fd >= 0){
    printf(1, "open .. for writing succeeded!\n");
    return 1;
  }
  fd = open(".", 0);
  if(write(fd, "x", 1) > 0){
    printf(1, "write . succeeded!\n");
    return 1;
  }
  close(fd);
  return 0;
}

int
bigfile(int seed)
{
  int fd, i, total, cc;
  printf(1, "%d\n", seed);
  char name[16];
  strcpy(name, "bigfile");
  name[7] = 'a' + seed % 26;
  name[8] = 0;

  unlink(name);
  fd = open(name, O_CREATE | O_RDWR);
  if(fd < 0){
    printf(1, "bigfile: cannot create %s", name);
    return 1;
  }

  // added more blocks for increased load
  int count = 100;
  int chunksz = 600; // multiple of gran
  int gran = 6;

  for(i = 0; i < count; i++){
    memset(buf, i, chunksz);
    if(write(fd, buf, chunksz) != chunksz){
      printf(1, "bigfile: write %s failed\n", name);
      return 1;
    }
  }
  close(fd);

  fd = open(name, 0);
  if(fd < 0){
    printf(1, "bigfile: cannot open %s\n", name);
    return 1;
  }
  total = 0;
  for(i = 0; ; i++){
    cc = read(fd, buf, chunksz/gran);
    if(cc < 0){
      printf(1, "bigfile: read %s failed\n", name);
      return 1;
    }
    if(cc == 0)
      break;
    if(cc != (chunksz/gran)){
      printf(1, "bigfile: short read %s\n", name);
      return 1;
    }
    if(buf[0] != i/gran || buf[(chunksz/gran) - 1] != i/gran){
      printf(1, "bigfile: read %s wrong data\n", name);
      return 1;
    }
    total += cc;
  }
  close(fd);

  if(total != count*chunksz){
    printf(1, "bigfile: read %s wrong total\n", name);
    return 1;
  }
  unlink(name);
  return 0;
}

// allocate MAXFILE memory and read/write files using it
int
bigfile2(int seed)
{
  printf(1, "%d\n", seed);
  char *wbuf;
  char *rbuf;
  int fd;

  int pid = fork();
  char name[16];
  strcpy(name, "bigfile2");
  name[8] = 'a' + seed % 26;
  name[9] = pid ? 'a' : 'b';
  name[10] = 0;

  wbuf = sbrk(MAXFILE);
  if(wbuf < 0) {
    printf(1, "bigfile2: sbrk wbuf failed\n");
    return 1;
  }

  rbuf = sbrk(MAXFILE);
  if(rbuf < 0) {
    printf(1, "bigfile2: sbrk rbuf failed\n");
    return 1;
  }

  // create a big file
  fd = open(name, O_CREATE|O_RDWR);
  if(fd < 0) {
    printf(1, "bigfile2: open(%s) failed\n", name);
    return 1;
  }
  memset(wbuf, seed, MAXFILE);
  if(write(fd, wbuf, MAXFILE) != MAXFILE) {
    printf(1, "bigfile2: write to %s failed\n", name);
    return 1;
  }

  close(fd);
  fd = open(name, O_RDONLY);
  if(fd < 0) {
    printf(1, "bigfile2: open(%s) failed\n", name);
    return 1;
  }

  if(read(fd, rbuf, MAXFILE) != MAXFILE) {
    printf(1, "bigfile2: read to %s failed\n", name);
    return 1;
  }

  for(int i = 0; i < MAXFILE; i++) {
    if(wbuf[i] != rbuf[i]) {
      printf(1, "bigfile2: diff %s failed\n", name);
      return 1;
    }
  }
  close(fd);
  if(pid == 0)
    exit();
  else
    wait();

  unlink(name);
  return 0;
}

int
fourteen(void)
{
  int fd;

  if(mkdir("12345678901234") != 0){
    printf(1, "fourteen: mkdir 12345678901234 failed\n");
    return 1;
  }
  if(mkdir("12345678901234/123456789012345") != 0){
    printf(1, "fourteen: mkdir 12345678901234/123456789012345 failed\n");
    return 1;
  }
  fd = open("123456789012345/123456789012345/123456789012345", O_CREATE);
  if(fd < 0){
    printf(1, "fourteen: create 123456789012345/123456789012345/123456789012345 failed\n");
    return 1;
  }
  close(fd);
  fd = open("12345678901234/12345678901234/12345678901234", 0);
  if(fd < 0){
    printf(1, "fourteen: open 12345678901234/12345678901234/12345678901234 failed\n");
    return 1;
  }
  close(fd);

  if(mkdir("12345678901234/12345678901234") == 0){
    printf(1, "fourteen: mkdir 12345678901234/12345678901234 succeeded!\n");
    return 1;
  }
  if(mkdir("123456789012345/12345678901234") == 0){
    printf(1, "fourteen: mkdir 12345678901234/123456789012345 succeeded!\n");
    return 1;
  }
  return 0;
}

int
subdir(void)
{
  int fd, cc;

  unlink("ff");
  if(mkdir("dd") != 0){
    printf(1, "subdir: mkdir dd failed\n");
    return 1;
  }

  fd = open("dd/ff", O_CREATE | O_RDWR);
  if(fd < 0){
    printf(1, "subdir: create dd/ff failed\n");
    return 1;
  }
  write(fd, "ff", 2);
  close(fd);

  if(unlink("dd") >= 0){
    printf(1, "subdir: unlink dd (non-empty dir) succeeded!\n");
    return 1;
  }

  if(mkdir("/dd/dd") != 0){
    printf(1, "subdir: subdir mkdir dd/dd failed\n");
    return 1;
  }

  fd = open("dd/dd/ff", O_CREATE | O_RDWR);
  if(fd < 0){
    printf(1, "subdir: create dd/dd/ff failed\n");
    return 1;
  }
  write(fd, "FF", 2);
  close(fd);

  fd = open("dd/dd/../ff", 0);
  if(fd < 0){
    printf(1, "subdir: open dd/dd/../ff failed\n");
    return 1;
  }
  cc = read(fd, buf, sizeof(buf));
  if(cc != 2 || buf[0] != 'f'){
    printf(1, "subdir: dd/dd/../ff wrong content\n");
    return 1;
  }
  close(fd);

  if(link("dd/dd/ff", "dd/dd/ffff") != 0){
    printf(1, "subdir: link dd/dd/ff dd/dd/ffff failed\n");
    return 1;
  }

  if(unlink("dd/dd/ff") != 0){
    printf(1, "subdir: unlink dd/dd/ff failed\n");
    return 1;
  }
  if(open("dd/dd/ff", O_RDONLY) >= 0){
    printf(1, "subdir: open (unlinked) dd/dd/ff succeeded\n");
    return 1;
  }

  if(chdir("dd") != 0){
    printf(1, "subdir: chdir dd failed\n");
    return 1;
  }
  if(chdir("dd/../../dd") != 0){
    printf(1, "subdir: chdir dd/../../dd failed\n");
    return 1;
  }
  if(chdir("dd/../../../dd") != 0){
    printf(1, "subdir: chdir dd/../../dd failed\n");
    return 1;
  }
  if(chdir("./..") != 0){
    printf(1, "subdir: chdir ./.. failed\n");
    return 1;
  }

  fd = open("dd/dd/ffff", 0);
  if(fd < 0){
    printf(1, "subdir: open dd/dd/ffff failed\n");
    return 1;
  }
  if(read(fd, buf, sizeof(buf)) != 2){
    printf(1, "subdir: read dd/dd/ffff wrong len\n");
    return 1;
  }
  close(fd);

  if(open("dd/dd/ff", O_RDONLY) >= 0){
    printf(1, "subdir: open (unlinked) dd/dd/ff succeeded!\n");
    return 1;
  }

  if(open("dd/ff/ff", O_CREATE|O_RDWR) >= 0){
    printf(1, "subdir: create dd/ff/ff succeeded!\n");
    return 1;
  }
  if(open("dd/xx/ff", O_CREATE|O_RDWR) >= 0){
    printf(1, "subdir: create dd/xx/ff succeeded!\n");
    return 1;
  }
  if(open("dd", O_CREATE) >= 0){
    printf(1, "subdir: create dd succeeded!\n");
    return 1;
  }
  if(open("dd", O_RDWR) >= 0){
    printf(1, "subdir: open dd rdwr succeeded!\n");
    return 1;
  }
  if(open("dd", O_WRONLY) >= 0){
    printf(1, "subdir: open dd wronly succeeded!\n");
    return 1;
  }
  if(link("dd/ff/ff", "dd/dd/xx") == 0){
    printf(1, "subdir: link dd/ff/ff dd/dd/xx succeeded!\n");
    return 1;
  }
  if(link("dd/xx/ff", "dd/dd/xx") == 0){
    printf(1, "subdir: link dd/xx/ff dd/dd/xx succeeded!\n");
    return 1;
  }
  if(link("dd/ff", "dd/dd/ffff") == 0){
    printf(1, "subdir: link dd/ff dd/dd/ffff succeeded!\n");
    return 1;
  }
  if(mkdir("dd/ff/ff") == 0){
    printf(1, "subdir: mkdir dd/ff/ff succeeded!\n");
    return 1;
  }
  if(mkdir("dd/xx/ff") == 0){
    printf(1, "subdir: mkdir dd/xx/ff succeeded!\n");
    return 1;
  }
  if(mkdir("dd/dd/ffff") == 0){
    printf(1, "subdir: mkdir dd/dd/ffff succeeded!\n");
    return 1;
  }
  if(unlink("dd/xx/ff") == 0){
    printf(1, "subdir: unlink dd/xx/ff succeeded!\n");
    return 1;
  }
  if(unlink("dd/ff/ff") == 0){
    printf(1, "subdir: unlink dd/ff/ff succeeded!\n");
    return 1;
  }
  if(chdir("dd/ff") == 0){
    printf(1, "subdir: chdir dd/ff succeeded!\n");
    return 1;
  }
  if(chdir("dd/xx") == 0){
    printf(1, "subdir: chdir dd/xx succeeded!\n");
    return 1;
  }

  if(unlink("dd/dd/ffff") != 0){
    printf(1, "subdir: unlink dd/dd/ff failed\n");
    return 1;
  }
  if(unlink("dd/ff") != 0){
    printf(1, "subdir: unlink dd/ff failed\n");
    return 1;
  }
  if(unlink("dd") == 0){
    printf(1, "subdir: unlink non-empty dd succeeded!\n");
    return 1;
  }
  if(unlink("dd/dd") < 0){
    printf(1, "subdir: unlink dd/dd failed\n");
    return 1;
  }
  if(unlink("dd") < 0){
    printf(1, "subdir: unlink dd failed\n");
    return 1;
  }
  return 0;
}

// can I unlink a file and still read it?
int
unlinkread(void)
{
  int fd, fd1;

  fd = open("unlinkread", O_CREATE | O_RDWR);
  if(fd < 0){
    printf(1, "create unlinkread failed\n");
    return 1;
  }
  write(fd, "hello", 5);
  close(fd);

  fd = open("unlinkread", O_RDWR);
  if(fd < 0){
    printf(1, "open unlinkread failed\n");
    return 1;
  }
  if(unlink("unlinkread") != 0){
    printf(1, "unlink unlinkread failed\n");
    return 1;
  }

  fd1 = open("unlinkread", O_CREATE | O_RDWR);
  write(fd1, "yyy", 3);
  close(fd1);

  if(read(fd, buf, sizeof(buf)) != 5){
    printf(1, "unlinkread read failed");
    return 1;
  }
  if(buf[0] != 'h'){
    printf(1, "unlinkread wrong data\n");
    return 1;
  }
  if(write(fd, buf, 10) != 10){
    printf(1, "unlinkread write failed\n");
    return 1;
  }
  close(fd);
  unlink("unlinkread");
  return 0;
}

int (*module_5[])(int) = {
  [0] opentest,
  [1] writetest,
  [2] writetest1,
  [3] createtest,
  [4] mem,
};

void
module5(void)
{
  int uptime0 = uptime();
  printf(1, "module 5 started\n");
  int ret, i;
  int pid;
  for(i = 0; i < NELEM(module_5)*nthreads; i++) {
    if((pid = fork()) == 0) {
      ret = module_5[i/nthreads](i%nthreads);
      if(ret) {
        dopanic("TEST FAILED\n");
      }
      exit();
    }
    else if(pid < 0) {
      dopanic("module5: fork failed");
    }
  }
  // wait for all children
  for(i = 0; i < NELEM(module_5)*nthreads; i++) {
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

int (*module_6[])(int) = {
  [0] pipe1,
  [1] uio,
  [2] forktest,
};

void
module6(void)
{
  int uptime0 = uptime();
  printf(1, "module 6 started\n");
  int ret, i;
  int pid;
  for(i = 0; i < NELEM(module_6)*nthreads; i++) {
    if((pid = fork()) == 0) {
      ret = module_6[i/nthreads](i%nthreads);
      if(ret) {
        dopanic("TEST FAILED\n");
      }
      else if(pid < 0) {
        dopanic("module6: fork failed");
      }
      exit();
    }
  }
  // wait for all children
  for(i = 0; i < NELEM(module_6)*nthreads; i++) {
    wait();
  }

  if((pid = fork()) == 0) {
    int ret = preempt(0);
    if(ret)
      dopanic("preempt failed\n");
    exit();
  }
  else if(pid < 0) {
    dopanic("module6: fork failed\n");
  }
  else {
    wait();
  }

  printf(1, "module 6 passed\n");
  printf(1, "module 6 time: %d\n", uptime() - uptime0);
}

int (*module_7[])(int) = {
  [0] rmdot,
  [1] dirfile,
  [2] bigfile,
  [3] bigfile2,
};

void
module7(void)
{
  int uptime0 = uptime();
  printf(1, "module 7 started\n");
  int ret, i;
  int pid;
  for(i = 0; i < NELEM(module_7)*nthreads; i++) {
    if((pid = fork()) == 0) {
      ret = module_7[i/nthreads](i%nthreads);
      if(ret) {
        dopanic("TEST FAILED\n");
      }
      exit();
    }
    else if(pid < 0) {
      dopanic("module7: fork failed");
    }
  }
  // wait for all children
  for(i = 0; i < NELEM(module_7)*nthreads; i++) {
    wait();
  }

  unlink("dirfile"); // dirfile

  printf(1, "module 7 passed\n");
  printf(1, "module 7 time: %d\n", uptime() - uptime0);
}

int (*module_8[])(int) = {
  [0] bigdir,
  [1] linktest,
  [2] iref,
};

void
module8(void)
{
  int uptime0 = uptime();
  printf(1, "module 8 started\n");
  int ret, i;
  int pid;
  for(i = 0; i < NELEM(module_8)*nthreads; i++) {
    if((pid = fork()) == 0) {
      ret = module_8[i/nthreads](i%nthreads);
      if(ret) {
        dopanic("TEST FAILED\n");
      }
      exit();
    }
    else if(pid < 0) {
      dopanic("module8: fork failed");
    }
  }
  // wait for all children
  for(i = 0; i < NELEM(module_8)*nthreads; i++) {
    wait();
  }

  for(i = 0; i < 3; i++) {
    if(fork() == 0) {
      switch(i) {
        case 0:
          if(fourteen()) {
            dopanic("TEST FAILED\n");
          }
          exit();
          break;
        case 1:
          if(subdir()) {
            dopanic("TEST FAILED\n");
          }
          exit();
          break;
        case 2:
          if(unlinkread()) {
            dopanic("TEST FAILED\n");
          }
          exit();
          break;
        default:
          break;
      }
    }
    else if(pid < 0) {
      dopanic("module8: fork failed");
    }
  }
  for(i = 0; i < 3; i++) {
    wait();
  }

  printf(1, "module 8 passed\n");
  printf(1, "module 8 time: %d\n", uptime() - uptime0);
}

int
main(int argc, char *argv[])
{
  nthreads = atoi(argv[1]);
  module5();
  module6();
  module7();
  module8();
  exit();
}
