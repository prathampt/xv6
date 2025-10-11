//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"

extern struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *curproc = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd] == 0){
      curproc->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

int
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

int
sys_read(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return fileread(f, p, n);
}

int
sys_write(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return filewrite(f, p, n);
}

int
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

int
sys_fstat(void)
{
  struct file *f;
  struct stat *st;

  if(argfd(0, 0, &f) < 0 || argptr(1, (void*)&st, sizeof(*st)) < 0)
    return -1;
  return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
int
sys_link(void)
{
  char name[DIRSIZ], *new, *old;
  struct inode *dp, *ip;

  if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
    return -1;

  begin_op();
  if((ip = namei(old)) == 0){
    end_op();
    return -1;
  }

  ilock(ip);
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

//PAGEBREAK!
int
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], *path;
  uint off;

  if(argstr(0, &path) < 0)
    return -1;

  begin_op();
  if((dp = nameiparent(path, name)) == 0){
    end_op();
    return -1;
  }

  ilock(dp);

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;
  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && ip->type == T_FILE)
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);

  return ip;
}

// IPC:
int ksend(int src_index, int dst_index, struct message *m) {
  struct proc *src, *dst;

  src = &ptable.proc[src_index];
  dst = &ptable.proc[dst_index];

  acquire(&ptable.lock);

  // check if the destination is LISTENING
  // this also implies that the queue of recieved messages is empty :)
  if(dst->state == LISTENING) {
    dst->recv_msg_queue = m;
    // can handle request immediately
    dst->state = RUNNABLE;
    release(&ptable.lock);
    return 0;
  }
  // need to append to the queue, dst is busy
  struct message **p = &dst->recv_msg_queue;

  // good taste in code :)
  while(*p)
    p = &(*p)->next;
  *p = m;

  src->state = BLOCKED;
  sched();
  release(&ptable.lock);
  return 0;
}

struct message *klisten(void) {
  struct proc *curproc = myproc();

  acquire(&ptable.lock);

  // queue is empty
  if(!curproc->recv_msg_queue) {
    // need to change state to LISTENING and call sched()
    curproc->state = LISTENING;
    sched();
  }

  // we are here means that someone enqueued a message while we were
  // LISTENING OR there was already a message in the queue
  
  // m points to the current message to be handled
  struct message *m = curproc->recv_msg_queue;
  curproc->recv_msg_queue = m->next;
  
  curproc->recv_proc = m->src;

  // make the src RUNNABLE if it is BLOCKED in ksend()
  if(m->src->state == BLOCKED) {
    m->src->state = RUNNABLE;
  }

  release(&ptable.lock);

  return m;
}

int krply(int src_index, int dst_index, struct message *m) {
  struct proc *src, *dst;

  src = &ptable.proc[src_index];
  dst = &ptable.proc[dst_index];

  acquire(&ptable.lock);

  dst->rply_msg = m;
  if(dst->state == BLOCKED) {
    dst->state = RUNNABLE;
  }

  src->recv_proc = 0;

  release(&ptable.lock);
  return 0;
}

struct message *krecv(void) {
  struct proc *curproc = myproc();

  acquire(&ptable.lock);

  // reply yet to come
  if(!curproc->rply_msg) {
    // need to change state to BLOCKED and call sched()
    curproc->state = BLOCKED;
    sched();
    // re acquire() lock when scheduled again
  }

  // we are here means that someone handled the request and called rply()
  // they changed our state to RUNNABLE in krply() if we had BLOCKED
  
  struct message *m = curproc->rply_msg;
  curproc->rply_msg = 0;
  
  release(&ptable.lock);

  return m;
}

#define MAXSTRSIZE 64

struct message *construct_msg(char *fmt, int vecnum, int argnum) {
  // kfree() in counterpart function
  struct message *m = (struct message *)kalloc();
  // print to the given buffer. Only understands d, p, s, b
  // b is a blob, which will be used when a large message payload like
  // a file's blocks are to be sent
  // blobs are also allocated by kalloc() and release by kfree()

  m->blob = 0;
  m->next = 0;
  struct proc *curproc = myproc();
  m->src = curproc;
  char *end_ptr = (char *)m->buf;

  // if this is called in krply(), then don't put vecnum and index
  if(vecnum != -1) {
    int *tmp = (int *)m->buf;
    tmp[0] = vecnum;
    tmp[1] = curproc - (struct proc *) &ptable.proc;
    end_ptr = (char *)&tmp[2];
  }

  int bad = 0;

  char c, *p, *str, *b;
  while ((c = *fmt++) && !bad) {
    switch (c) {
      case 'd': case 'p': // assuming that sizeof(int) == sizeof(void *)
        if (argint(argnum, (int *)end_ptr) < 0) {
          bad = 1;
        }
        end_ptr += sizeof(int);
        break;
      case 's':
        if (argstr(argnum, &str) == -1) {
          bad = 1;
          break;
        }
        while (*end_ptr++ = *str++)
          ;
        break;
      case 'b':
        if (argptr(argnum, &p, PGSIZE) < 0) { // TODO: modify/change argptr
          bad = 1;
          break;
        }
        m->blob = kalloc();
        b = m->blob;
        for (int i = 0; i < PGSIZE; i++) {
          *b++ = *p++;
        }
        break;
      default:
        bad = 1;
        break;
    }
    argnum++;
  }

  if (bad) {
    if (m->blob) {
      kfree(m->blob);
    }
    kfree((char *)m);
    return 0;
  }

  return m;
}

int deconstruct_msg(char *fmt, int argnum, struct message * m) {
  char c;
  int bad = 0;
  int *ptr = 0;
  char *str = 0;
  char *end_ptr = m->buf;
  char *b = m->blob;
 
  while ((c = *fmt++) && !bad) {
    switch (c) {
      case 'd': case 'p':
        if (argptr(argnum, &ptr, sizeof(int)) < 0) {
          bad = 1;
          break;
        }
        *ptr = *(int *)end_ptr; 
        end_ptr += sizeof(int);
        break;
      case 's':
        if (argptr(argnum, &str, MAXSTRSIZE) < 0) {
          bad = 1;
          break;
        }
        while (*str++ = *end_ptr++)
          ;
        break;
      case 'b':
        if (argptr(argnum, &str, PGSIZE) < 0 || !b) {
          bad = 1;
          break;
        }
        for (int i = 0; i < PGSIZE; i++) {
          *str++ = *b++;
        }
        break;
    }
    argnum++;
  }

  if (m->blob) {
    cprintf("freeing blob in deconstruct_msg()\n");
    kfree(m->blob);
  }
  cprintf("freeing msg in deconstruct_msg()\n");
  kfree((char *)m);

  return bad;
}

// sys_ for IPC
int sys_send(void) {
  struct proc *curproc = myproc();
  int dst_proc, vecnum;
  char *fmt;

  // fetch dst_proc from the user
  if(argint(0, &dst_proc) < 0 || argint(1, &vecnum) < 0 || argstr(2, &fmt) < 0) {
    return -1;
  }
  int argnum = 3;

  struct message *m = construct_msg(fmt, vecnum, argnum);
  if(!m) {
    return -1;
  }

  return ksend(curproc - (struct proc *) &ptable.proc, dst_proc, m);
}

// sys_listen()
int sys_listen(void) {
  struct message *m = klisten();
  char *fmt;

  if(argstr(0, &fmt) < 0) {
    return -1;
  }
  int argnum = 1;

  // TODO: need to see how this deconstruct_msg() will work
  return deconstruct_msg(fmt, argnum, m);
}

#define IMPLICIT -1
// sys_rply()
// rply(IMPLICIT/EXPLICIT, char *fmt, args)
int sys_rply(void) {

  struct proc *curproc = myproc();
  int dst_proc;
  char *fmt;

  // fetch dst_proc from the user
  if(argint(0, &dst_proc) < 0 || argstr(1, &fmt) < 0) {
    return -1;
  }

  // use existing pointers if IMPLICIT
  if(dst_proc == IMPLICIT) {
    dst_proc = curproc->recv_proc - (struct proc *) &ptable.proc;
  }

  int argnum = 2;

  // passing -1 so vecnum and index will not be included in the message
  struct message *m = construct_msg(fmt, -1, argnum);

  if(!m) {
    return -1;
  }

  return krply(curproc - (struct proc *) &ptable.proc, dst_proc, m);
}

// recv() 
// int recv(char *fmt, ...addresses of the variables where we want to store the content of buffer...);

int sys_recv(void) {
  struct message *m = krecv();
  char *fmt;

  if(argstr(0, &fmt) < 0) {
    return -1;
  }
  int argnum = 1;

  return deconstruct_msg(fmt, argnum, m);
}


int
sys_open(void)
{
  char *path;
  int fd, omode;
  struct file *f;
  struct inode *ip;

  if(argstr(0, &path) < 0 || argint(1, &omode) < 0)
    return -1;

  begin_op();

  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  end_op();

  f->type = FD_INODE;
  f->ip = ip;
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
  return fd;
}

int
sys_mkdir(void)
{
  char *path;
  struct inode *ip;

  begin_op();
  if(argstr(0, &path) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_mknod(void)
{
  struct inode *ip;
  char *path;
  int major, minor;

  begin_op();
  if((argstr(0, &path)) < 0 ||
     argint(1, &major) < 0 ||
     argint(2, &minor) < 0 ||
     (ip = create(path, T_DEV, major, minor)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_chdir(void)
{
  char *path;
  struct inode *ip;
  struct proc *curproc = myproc();
  
  begin_op();
  if(argstr(0, &path) < 0 || (ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  iput(curproc->cwd);
  end_op();
  curproc->cwd = ip;
  return 0;
}

int
sys_exec(void)
{
  char *path, *argv[MAXARG];
  int i;
  uint uargv, uarg;

  if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv))
      return -1;
    if(fetchint(uargv+4*i, (int*)&uarg) < 0)
      return -1;
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    if(fetchstr(uarg, &argv[i]) < 0)
      return -1;
  }
  return exec(path, argv);
}

int
sys_pipe(void)
{
  int *fd;
  struct file *rf, *wf;
  int fd0, fd1;

  if(argptr(0, (void*)&fd, 2*sizeof(fd[0])) < 0)
    return -1;
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      myproc()->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  fd[0] = fd0;
  fd[1] = fd1;
  return 0;
}
