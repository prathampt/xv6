// sh.c - xv6 shell with history, '*' wildcard, and up-arrow history recall
// based on the original xv6 sh.c with minimal invasive changes.

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "fs.h"

#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

#define MAXARGS 10

// xv6 does not provide strncpy in userland; provide a small implementation.
static char*
strncpy(char *dst, const char *src, int n)
{
  char *d = dst;
  int i;
  for(i = 0; i < n && src[i]; i++)
    d[i] = src[i];
  /* if src shorter than n, pad with '\0' */
  for(; i < n; i++)
    d[i] = '\0';
  return dst;
}

int
strncmp(const char *p, const char *q, uint n)
{
  while (n > 0 && *p && *p == *q) {
    n--;
    p++;
    q++;
  }

  if (n == 0)
    return 0;
  return (unsigned char)*p - (unsigned char)*q;
}

/* copy string with size limit, always null-terminating */
char*
safestrcpy(char *dst, const char *src, int n)
{
  char *os = dst;
  if(n <= 0)
    return os;
  while(--n > 0 && (*dst++ = *src++) != 0)
    ;
  *dst = 0;
  return os;
}


struct cmd {
  int type;
};

struct execcmd {
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

struct redircmd {
  int type;
  struct cmd *cmd;
  char *file;
  char *efile;
  int mode;
  int fd;
};

struct pipecmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd {
  int type;
  struct cmd *cmd;
};

int fork1(void);  // Fork but panics on failure.
void panic(char*);
struct cmd *parsecmd(char*);

// Execute cmd.  Never returns.
void
runcmd(struct cmd *cmd)
{
  int p[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    exit();

  switch(cmd->type){
  default:
    panic("runcmd");

  case EXEC:
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0)
      exit();
    exec(ecmd->argv[0], ecmd->argv);
    printf(2, "exec %s failed\n", ecmd->argv[0]);
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    close(rcmd->fd);
    if(open(rcmd->file, rcmd->mode) < 0){
      printf(2, "open %s failed\n", rcmd->file);
      exit();
    }
    runcmd(rcmd->cmd);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    if(fork1() == 0)
      runcmd(lcmd->left);
    wait();
    runcmd(lcmd->right);
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    if(pipe(p) < 0)
      panic("pipe");
    if(fork1() == 0){
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->left);
    }
    if(fork1() == 0){
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
    close(p[0]);
    close(p[1]);
    wait();
    wait();
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    if(fork1() == 0)
      runcmd(bcmd->cmd);
    break;
  }
  exit();
}

/* --------------------------
   NEW: history + line editing
   -------------------------- */
#define HISTORY_SIZE 50
#define LINE_MAX 128

static char history[HISTORY_SIZE][LINE_MAX];
static int hist_count = 0;   // number of stored entries (<= HISTORY_SIZE)
static int hist_next = 0;    // next index to write (circular)
static int hist_nav_idx = 0; // navigation index when pressing up

// add a non-empty line (without trailing '\n') to history
static void
history_add(char *line)
{
  int len = strlen(line);
  if(len == 0) return;
  // store at hist_next
  strncpy(history[hist_next], line, LINE_MAX-1);
  history[hist_next][LINE_MAX-1] = 0;
  hist_next = (hist_next + 1) % HISTORY_SIZE;
  if(hist_count < HISTORY_SIZE)
    hist_count++;
}

// get history entry by reverse index: 0 -> most recent, 1 -> previous, ...
// returns pointer to internal history buffer or 0 if out of bounds
static char*
history_get_reverse(int rev)
{
  if(rev < 0 || rev >= hist_count) return 0;
  int idx = (hist_next - 1 - rev);
  while(idx < 0) idx += HISTORY_SIZE;
  return history[idx];
}

/* --------------------------
   End history helpers
   -------------------------- */

/*
  Simple wildcard matcher supporting only '*' (matches zero or more characters).
  returns 1 if pattern matches name, 0 otherwise.
*/
static int
match_star(const char *pattern, const char *name)
{
  // recursive implementation
  if(*pattern == '\0') return *name == '\0';
  if(*pattern == '*'){
    // try to match zero or more chars
    // advance pattern past consecutive '*'
    while(*pattern == '*') pattern++;
    if(*pattern == '\0') return 1; // trailing * matches everything
    // try matching name at every possible position
    for(; *name; name++){
      if(match_star(pattern, name)) return 1;
    }
    return 0;
  } else {
    if(*name == '\0') return 0;
    if(*pattern == *name)
      return match_star(pattern+1, name+1);
    return 0;
  }
}

/*
  Expand a single token containing '*' by reading current directory and
  returning matches in results[]; *nresults set accordingly.
  Caller should ensure results has enough slots.
  Returns 0 on success.
*/
static int
expand_wildcard(const char *pattern, char **results, int *nresults)
{
  int fd;
  struct dirent de;
  char buf[DIRSIZ+1];
  int nr = 0;

  fd = open(".", O_RDONLY);
  if(fd < 0) return -1;
  while(read(fd, &de, sizeof(de)) == sizeof(de)){
    if(de.inum == 0) continue;
    // name isn't null-terminated in de.name
    memmove(buf, de.name, DIRSIZ);
    buf[DIRSIZ] = 0;
    // trim trailing spaces or zeroes
    int i;
    for(i = DIRSIZ-1; i >= 0; i--){
      if(buf[i] == 0 || buf[i] == '\0' || buf[i] == ' ') buf[i] = 0;
      else break;
    }
    if(buf[0] == 0) continue;
    if(match_star(pattern, buf)){
      // allocate and copy matched name
      int l = strlen(buf);
      char *s = malloc(l+1);
      if(!s) { close(fd); return -1; }
      strcpy(s, buf);
      results[nr++] = s;
      if(nr >= MAXARGS) break; // limit to MAXARGS expansions per token
    }
  }
  close(fd);
  *nresults = nr;
  return 0;
}

/*
  Read a line from console into buf, handle backspace, handle up-arrow to
  recall history. buf will include trailing '\n' and be nul-terminated.
  Returns 0 normally, -1 on EOF.
*/
int
getcmd(char *buf, int nbuf)
{
  char prompt[] = "$ ";
  int prompt_len = strlen(prompt);
  int pos = 0;
  int prev_len = 0; // previous printed line length (for clearing)
  char c;
  int r;
  int nav_rev = -1; // -1 means not actively navigating; 0 means most recent

  // print prompt
  printf(2, "%s", prompt);
  // clear buffer
  memset(buf, 0, nbuf);

  while(1){
    r = read(0, &c, 1);
    if(r <= 0) {
      // EOF or read error
      if(pos == 0) return -1;
      // otherwise treat as newline
      c = '\n';
    }

    if(c == '\r') c = '\n';

    if(c == 0x1b) {
      // escape sequence: try to read two more bytes
      char seq[2];
      if(read(0, &seq[0], 1) <= 0) continue;
      if(read(0, &seq[1], 1) <= 0) continue;
      // Up arrow is ESC [ A  (seq[0] = '[', seq[1] = 'A')
      if(seq[0] == '[' && seq[1] == 'A'){
        // up arrow pressed: navigate history older
        if(hist_count == 0) continue;
        if(nav_rev == -1) nav_rev = 0;
        else nav_rev++;
        if(nav_rev >= hist_count) nav_rev = hist_count - 1;
        char *h = history_get_reverse(nav_rev);
        if(!h) continue;
        // overwrite current line: print carriage return, prompt and history entry
        // To clear leftovers from prior longer input, print spaces over remainder.
        // First compute lengths
        int newlen = strlen(h);
        // move cursor to beginning of line
        printf(2, "\r");
        // print prompt + entry
        printf(2, "%s", prompt);
        printf(2, "%s", h);
        // if previous line was longer, overwrite remainder with spaces
        if(prev_len > newlen + prompt_len){
          int extra = prev_len - (newlen + prompt_len);
          // print spaces
          int i;
          for(i=0;i<extra;i++) printf(2, " ");
          // move cursor back to beginning and reprint prompt+entry so cursor is after text
          printf(2, "\r");
          printf(2, "%s", prompt);
          printf(2, "%s", h);
        }
        // set buffer to history content
        pos = newlen;
        if(pos >= nbuf-1) pos = nbuf-2;
        memmove(buf, h, pos);
        buf[pos] = 0;
        prev_len = prompt_len + pos;
        continue;
      } else {
        // ignore other escape sequences (left/right/down etc.)
        continue;
      }
    }

    if(c == '\n'){
      // echo newline
      printf(2, "\n");
      buf[pos] = '\n';
      buf[pos+1] = 0;
      // if not empty (only newline), add to history
      if(pos > 0){
        // store without trailing newline
        char tmpline[LINE_MAX];
        int l = pos;
        if(l >= LINE_MAX) l = LINE_MAX-1;
        memmove(tmpline, buf, l);
        tmpline[l] = 0;
        history_add(tmpline);
      }
      // reset nav index
      hist_nav_idx = 0;
      return 0;
    }

    // Backspace handling: 8 or 127
    if(c == 0x7f || c == 0x08){
      if(pos > 0){
        pos--;
        buf[pos] = 0;
        // move cursor back, print space, move back (rudimentary)
        printf(2, "\b \b");
        prev_len--;
      }
      nav_rev = -1; // cancel history navigation on edit
      continue;
    }

    // regular printable characters
    if(c >= 32 && c <= 126){
      if(pos < nbuf-2){
        buf[pos++] = c;
        buf[pos] = 0;
        // echo
        char tmp[2];
        tmp[0] = c; tmp[1] = 0;
        printf(2, "%s", tmp);
        prev_len++;
      }
      nav_rev = -1; // cancel history navigation on edit
      continue;
    }

    // ignore other bytes
  }

  // unreachable
  return -1;
}

int
main(void)
{
  static char buf[LINE_MAX];
  int fd;

  // Ensure that three file descriptors are open.
  while((fd = open("console", O_RDWR)) >= 0){
    if(fd >= 3){
      close(fd);
      break;
    }
  }

  // Read and run input commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
    // buf contains a trailing '\n'; keep same behavior as original
    if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
      // Chdir must be called by the parent, not the child.
      buf[strlen(buf)-1] = 0;  // chop \n
	  if(chdir(buf+3) < 0)
		  printf(2, "cannot cd %s\n", buf+3);
	  continue;
	} 
	if(fork1() == 0)
		runcmd(parsecmd(buf));
	wait();
  }
  exit();
}

void
panic(char *s)
{
  printf(2, "%s\n", s);
  exit();
}

int
fork1(void)
{
  int pid;

  pid = fork();
  if(pid == -1)
    panic("fork");
  return pid;
}

//PAGEBREAK!
// Constructors

struct cmd*
execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd*)cmd;
}

struct cmd*
redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd*)cmd;
}

struct cmd*
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
listcmd(struct cmd *left, struct cmd *right)
{
  struct listcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
backcmd(struct cmd *subcmd)
{
  struct backcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd*)cmd;
}
//PAGEBREAK!
// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int
gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  if(q)
    *q = s;
  ret = *s;
  switch(*s){
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++;
    break;
  case '>':
    s++;
    if(*s == '>'){
      ret = '+';
      s++;
    }
    break;
  default:
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if(eq)
    *eq = s;

  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int
peek(char **ps, char *es, char *toks)
{
  char *s;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);
struct cmd *parseredirs(struct cmd*, char**, char*);
struct cmd *parseblock(char**, char*);
struct cmd *nulterminate(struct cmd*);

struct cmd*
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if(s != es){
    printf(2, "leftovers: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd);
  return cmd;
}

struct cmd*
parseline(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parsepipe(ps, es);
  while(peek(ps, es, "&")){
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  if(peek(ps, es, ";")){
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

struct cmd*
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

/*
  Modified parseexec to expand '*' wildcards. If a token contains '*',
  it is expanded by listing files in current directory and matching them.
  Matches are added into argv. Memory for expanded names is allocated
  with malloc so nulterminate() can safely write terminating '\0'.
*/
struct cmd*
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;

  if(peek(ps, es, "("))
    return parseblock(ps, es);

  ret = execcmd();
  cmd = (struct execcmd*)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while(!peek(ps, es, "|)&;")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a')
      panic("syntax");

    // q..eq points into the input buffer; create a temporary nul-terminated string
    int len = eq - q;
    if(len <= 0) continue;
    char tmp[LINE_MAX];
    if(len >= LINE_MAX) len = LINE_MAX-1;
    memmove(tmp, q, len);
    tmp[len] = 0;

    // if token contains '*' we expand it
    int has_star = 0;
    int i;
    for(i=0; tmp[i]; i++) if(tmp[i] == '*') { has_star = 1; break; }

    if(has_star){
      // attempt expansion
      char *matches[MAXARGS];
      int nmatches = 0;
      if(expand_wildcard(tmp, matches, &nmatches) == 0 && nmatches > 0){
        // add matches to argv
        int j;
        for(j=0; j<nmatches && argc < MAXARGS-1; j++){
          cmd->argv[argc] = matches[j];
          cmd->eargv[argc] = matches[j] + strlen(matches[j]);
          argc++;
        }
      } else {
        // no matches: treat pattern as literal (common shell may keep literal or remove; we keep literal)
        char *s = malloc(len+1);
        if(!s) panic("malloc");
        strcpy(s, tmp);
        cmd->argv[argc] = s;
        cmd->eargv[argc] = s + strlen(s);
        argc++;
      }
    } else {
      // no wildcard: point into input buffer (original behaviour expects eargv to be writable by nulterminate)
      // Allocate a writable copy so nulterminate can write '\0'.
      char *s = malloc(len+1);
      if(!s) panic("malloc");
      memmove(s, q, len);
      s[len] = 0;
      cmd->argv[argc] = s;
      cmd->eargv[argc] = s + len;
      argc++;
    }

    if(argc >= MAXARGS)
      panic("too many args");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

struct cmd*
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a')
      panic("missing file for redirection");
    switch(tok){
    case '<':
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1);
      break;
    case '+':  // >>
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1);
      break;
    }
  }
  return cmd;
}

struct cmd*
parseblock(char **ps, char *es)
{
  struct cmd *cmd;

  if(!peek(ps, es, "("))
    panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if(!peek(ps, es, ")"))
    panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

// NUL-terminate all the counted strings.
struct cmd*
nulterminate(struct cmd *cmd)
{
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    return 0;

  switch(cmd->type){
  case EXEC:
    ecmd = (struct execcmd*)cmd;
    for(i=0; ecmd->argv[i]; i++)
      *ecmd->eargv[i] = 0;
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    nulterminate(bcmd->cmd);
    break;
  }
  return cmd;
}
