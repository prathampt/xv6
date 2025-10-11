#include "types.h"
#include "stat.h"
#include "user.h"
char sendblob[4096];
char recvblob[4096];
char listenblob[4096];
char rplyblob[4096];

int
main(int argc, char *argv[])
{
  printf(1, "refer: sendptr should be: %p\n", sendblob);
  printf(1, "refer: rplyptr should be: %p\n", rplyblob);
  if(fork()) {
    printf(1, "parent\n");
    strcpy(sendblob + 100, "pratham_arjun :)");
    int *sendptr = sendblob;
    // parent
    send(4, 256, "dspb", -16, "send worked!!!", sendptr, sendblob);

    char *recvptr;
    char recvstr[64];
    int recvint;
    recv("psdb", &recvptr, &recvstr, &recvint, &recvblob);
    printf(1, "parent: recieved recvptr: %p\n", recvptr);
    printf(1, "parent: recieved recvstr: %s\n", recvstr);
    printf(1, "parent: recieved recvint: %d\n", recvint);
    printf(1, "parent: recieved recvblob:\n");
    for(int i = 0; i < 4096; i++) {
      printf(1, "%c", recvblob[i]);
    }
    printf(1, "\n\n");
    wait();
    printf(1, "code workedddddd\n-----------------------------------------------------\n\n\n");
  }
  else {
    printf(1, "child\n");
    
    int listenint;
    char *listenptr;
    char listenstr[64];
    int vecnum, index;
    listen("dddspb", &vecnum, &index, &listenint, &listenstr, &listenptr, &listenblob);
    printf(1, "child: recieved vecnum: %d\n", vecnum);
    printf(1, "child: recieved index: %d\n", index);
    printf(1, "child: recieved listenptr: %p\n", listenptr);
    printf(1, "child: recieved listenstr: %s\n", listenstr);
    printf(1, "child: recieved listenint: %d\n", listenint);
    printf(1, "child: recieved listenblob:\n");
    for(int i = 0; i < 4096; i++) {
      printf(1, "%c", listenblob[i]);
    }
    printf(1, "\n\n");

    strcpy(rplyblob + 100, "arjun_pratham :)");
    int *rplyptr = rplyblob;
    // parent
    rply(-1, "psdb", rplyptr, "rply worked!!!", -17, rplyblob);
  }
  exit();
}
