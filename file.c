//
// File descriptors
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"

#define MAXFILE_PERPAGE 160 // Calculated for 4KB page

struct devsw devsw[NDEV];

struct fileslab {
    int freecount;
    uint bitmap[5]; // to represent 5*4*8 = 160 file structures
    struct fileslab *next;
    struct fileslab *prev;
    int padding[64 - 8]; // (4k - 160 * 24) = 256 and 256 / 4 = 64
    struct file files[MAXFILE_PERPAGE];
}; // Size of the struct is 4KB :)

struct {
  struct spinlock lock;
  struct fileslab *head;
} ftable;

struct fileslab *get_file_slab(){
    struct fileslab *fileslab = (struct fileslab *)kalloc();
    fileslab->freecount = MAXFILE_PERPAGE;
    for (int i = 0; i < 5; i++) {
        fileslab->bitmap[i] = 0;
    }
    fileslab->next = 0;
    fileslab->prev = 0;

    return fileslab;
}

void
fileinit(void)
{
  initlock(&ftable.lock, "ftable");
  struct fileslab *new_slab = get_file_slab();
  new_slab->next = new_slab;
  new_slab->prev = new_slab;
  acquire(&ftable.lock);
  ftable.head = new_slab;
  release(&ftable.lock);
}

// Allocate a file structure.
struct file*
filealloc(void)
{
  struct file *f;
  struct fileslab *slab;
  acquire(&ftable.lock);
  if(!ftable.head)
      panic("filealloc: not initialized ");
  if(ftable.head->freecount == 0) {
      struct fileslab *new_slab = get_file_slab();
      new_slab->prev = ftable.head;
      new_slab->next = ftable.head->next;
      ftable.head->next = new_slab;
      new_slab->next->prev = new_slab;
      ftable.head = new_slab;
  }
  slab = ftable.head;
  for (int i = 0; i < 5; i++) {
      for (int j = 0; j < 32; j++) {
          uint mask = 1U << j;
          if (!(slab->bitmap[i] & mask)) {
              slab->bitmap[i] |= mask;
              slab->freecount--;
              f = &slab->files[i * 32 + j];
              f->ref = 1;
              release(&ftable.lock);
              return f;
          }
      }
  }
  panic("filealloc: ");
  return 0;
}

// Increment ref count for file f.
struct file*
filedup(struct file *f)
{
  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("filedup");
  f->ref++;
  release(&ftable.lock);
  return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void
fileclose(struct file *f)
{
  struct file ff;
  struct fileslab *slabaddr;
  int free = 0;

  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("fileclose");
  if(--f->ref > 0){
    release(&ftable.lock);
    return;
  }
  ff = *f;
  f->ref = 0;
  // Violating some laws under the name of optimization
  // Find addr of parent struct
  // As each page is aligned at 4KB boundary
  slabaddr = (struct fileslab *)((int)f & ~0xFFF);
  int index = f - slabaddr->files;

  if (index < 0 || index >= MAXFILE_PERPAGE)
      panic("invalid file pointer");
  slabaddr->bitmap[index / 32] &= ~(1U << (index % 32));
  if (slabaddr->freecount == MAXFILE_PERPAGE && slabaddr->next != slabaddr) {
      free = 1;
      ftable.head = slabaddr->prev;
      slabaddr->prev->next = slabaddr->next;
      slabaddr->next->prev = slabaddr->prev;
  }
  release(&ftable.lock);

  if(free)
      kfree((char *)slabaddr);

  if(ff.type == FD_PIPE)
    pipeclose(ff.pipe, ff.writable);
  else if(ff.type == FD_INODE){
    begin_op();
    iput(ff.ip);
    end_op();
  }
}

// Get metadata about file f.
int
filestat(struct file *f, struct stat *st)
{
  if(f->type == FD_INODE){
    ilock(f->ip);
    stati(f->ip, st);
    iunlock(f->ip);
    return 0;
  }
  return -1;
}

// Read from file f.
int
fileread(struct file *f, char *addr, int n)
{
  int r;

  if(f->readable == 0)
    return -1;
  if(f->type == FD_PIPE)
    return piperead(f->pipe, addr, n);
  if(f->type == FD_INODE){
    ilock(f->ip);
    if((r = readi(f->ip, addr, f->off, n)) > 0)
      f->off += r;
    iunlock(f->ip);
    return r;
  }
  panic("fileread");
}

//PAGEBREAK!
// Write to file f.
int
filewrite(struct file *f, char *addr, int n)
{
  int r;

  if(f->writable == 0)
    return -1;
  if(f->type == FD_PIPE)
    return pipewrite(f->pipe, addr, n);
  if(f->type == FD_INODE){
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((MAXOPBLOCKS-1-1-2) / 2) * 512;
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      begin_op();
      ilock(f->ip);
      if ((r = writei(f->ip, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      end_op();

      if(r < 0)
        break;
      if(r != n1)
        panic("short filewrite");
      i += r;
    }
    return i == n ? n : -1;
  }
  panic("filewrite");
}

