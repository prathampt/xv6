#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "x86.h"

// Mutual exclusion lock.
struct uspinlock {
  uint locked;       // Is the lock held?

  // For debugging:
  char *name;        // Name of lock.
};


void
uinitlock(struct uspinlock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0;
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
// Holding a lock for a long time may cause
// other CPUs to waste time spinning to acquire it.
void
uacquire(struct uspinlock *lk)
{
  if(holding(lk))
    printf(1, "acquire");

  // The xchg is atomic.
  while(xchg(&lk->locked, 1) != 0)
    ;

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that the critical section's memory
  // references happen after the lock is acquired.
  __sync_synchronize();
}

// Release the lock.
void
urelease(struct uspinlock *lk)
{
  if(!holding(lk))
    printf(1, "release");

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that all the stores in the critical
  // section are visible to other cores before the lock is released.
  // Both the C compiler and the hardware may re-order loads and
  // stores; __sync_synchronize() tells them both not to.
  __sync_synchronize();

  // Release the lock, equivalent to lk->locked = 0.
  // This code can't use a C assignment, since it might
  // not be atomic. A real OS would use C atomics here.
  asm volatile("movl $0, %0" : "+m" (lk->locked) : );
}

// Check whether this cpu is holding the lock.
int
holding(struct uspinlock *lock)
{
  return lock->locked;
}
struct global {
  struct uspinlock lk;
  int tmp;
  char arr[16];
  char buf[4];
  char *childbuf;
} global;


int
newfunc(void)
{
  sleep(10);
  printf(1, "I am new function and if you are seeing this then the clone is working!!! :)\n");
  uacquire(&global.lk);
  printf(1, "Child acquired the lock\n");
  printf(1, "child: arr[0]: %c, tmp: %d, buf[0]: %c\n", global.arr[0], global.tmp, global.buf[0]);
  global.arr[0] = 'b';
  global.tmp = 87;
  global.buf[0] = 'q';

  global.childbuf = (char *) malloc(1);
  global.childbuf[0] = 'z';
  printf(1, "printing in child: %c\n", global.childbuf[0]);
  printf(1, "child: arr[0]: %c, tmp: %d, buf[0]: %c\n", global.arr[0], global.tmp, global.buf[0]);
  urelease(&global.lk);
  printf(1, "Child released lock\n");
  exit();
  return 0;
}

int
main(int argc, char *argv[])
{
  uinitlock(&global.lk, "lock");
  global.arr[0] = 'a';
  global.tmp = 42;

  // global.buf = (char *) malloc(128);
  global.buf[0] = 'p';
  printf(1, "arr[0]: %c, tmp: %d, buf[0]: %c\n", global.arr[0], global.tmp, global.buf[0]);
  printf(1, "parent pid: %d\n", getpid());
  // int pid = fork();
  int pid = clone(newfunc);
  if(pid == 0) {
    newfunc();
    exit();
  }

  /*
  int wpid;
  wpid = 0;
  printf(1, "calling wait()\n");
  if((wpid=wait()) < 0) {
    printf(1, "wait failed\n");
  }
  printf(1, "called wait()\n");

  if(wpid != pid) {
    printf(1, "zombie\n");
  }
  */
  for(int i = 0; i < 10; i++) {
    sleep(5);
    uacquire(&global.lk);
    printf(1, "Parent acquired the lock\n");
    printf(1, "arr[0]: %c, tmp: %d, buf[0]: %c\n", global.arr[0], global.tmp, global.buf[0]);
    printf(1, "childbuf: %p\n", global.childbuf);
    urelease(&global.lk);
  }

  wait();
  printf(1, "dereferencing childbuf: should give segfault\n");
  printf(1, "printing in parent: %c\n", global.childbuf[0]);
  printf(1, "I the old function: I should be printed only once !!! :)\n");
  exit();
}
