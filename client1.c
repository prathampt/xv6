#include "types.h"
#include "stat.h"
#include "user.h"
#include "clientser.h"

int open_log(int index) {
	char str[16];
	str[0] = '0';
	strcpy(str, "clilog");
	int last = 6;
	int tmp = index;
	while(tmp) {
		str[last++] = (tmp % 10) + '0';
		str[last] = '\0';
		tmp /= 10;
	}
	int fd;
	if((fd = open(str, 0x001 | 0x200)) != -1) {
		printf(1, "[%d] client %d: created logfile %s: fd %d\n", uptime(), index, str, fd);
	}
	else {
		printf(1, "[%d] client %d: open(%s) failed\n", uptime(), index, str); 
		exit();
	}
}

int
main(int argc, char *argv[])
{
  fork();
  fork();
  fork();

  int index = getindex();
  open_log(index);
  int key, value;

  key = index;
  value = index * index + 1;

  int server_index = atoi(argv[1]);
  printf(3, "[%d] client: created client at index %d\n", uptime(), index);
  send(server_index, PUT_KVAL, "dd", key, value);

  printf(3, "[%d] client %d: sent message to store pair <%d, %d> to server\n", uptime(), index, key, value);

  int myslot;
  int status;
  recv("dd", &status, &myslot);
  printf(3, "[%d] client %d: server returned status %d, reserved slot at %d\n", uptime(), index, status, myslot);


  send(server_index, GET_KVAL, "ddd", index, key, value);
  printf(3, "[%d] client %d: sent message to get <%d, ?> to server\n", uptime(), index, key);
  recv("dd", &status, &value);
  printf(3, "[%d] client %d: got answer <%d, ?> = <%d, %d>\n", uptime(), index, key, key, value);

  /*
  if(index == 5) {
	  sleep(100);
	  send(server_index, -1, "ddd", index, key, value);
	  printf(3, "[%d] client %d: sent invalid message to server\n", uptime(),  index);
	  recv("dd", &status, &value);
	  printf(3, "[%d] client %d: got invalid response: %d\n", uptime(), index, status);
  }
  */

  while(1) {
	  int sleeptime = randomrange(1, 20);
	  sleep(sleeptime);
	  printf(3, "[%d] client %d: slept for %d\n", uptime(),  index, sleeptime);
	  send(server_index, -1, "ddd", index, key, value);
	  printf(3, "[%d] client %d: sent invalid message to server\n", uptime(),  index);
	  recv("dd", &status, &value);
	  printf(3, "[%d] client %d: got invalid response: %d\n", uptime(), index, status);
  }
  exit();
}
