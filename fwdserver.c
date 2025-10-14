// forwards message to main server whose index is given by argv[1]
// some invalid requests can be discarded here itself
#include "types.h"
#include "stat.h"
#include "user.h"
#include "clientser.h"

#define IMPLICIT -1

int
main(int argc, char *argv[])
{
	printf(1, "[%d] forwarding server: initialising at index %d\n", uptime(), getindex());
	int numproc;
	int curproc_index;
	int curproc;
	int vecnum;
	int slot;
	int value;
	int index;
	int key;
	int status;

	numproc = 0;
	int mainserver = atoi(argv[1]);

	int fd = open("fwdserverlog", 0x001 | 0x200);
	if(fd == -1) {
		printf(1, "[%d] open(fwdserverlog) failed\n", uptime());
		exit();
	}

	printf(3, "[%d] forward server: initialisation complete\n", uptime());
	while(1) {

		printf(3, "[%d] forward server: listening on index %d\n", uptime(), getindex());
		vecnum = listen();
		extractargs("dddd", &vecnum, &curproc_index, &key, &value);
		printf(3, "[%d] forward server: recieved request %d from process index %d\n", uptime(), vecnum, curproc_index); 
		switch(vecnum) {
			case PUT_KVAL:
				printf(3, "[%d] forward server: forwarding message to put pair <%d, %d> of process %d to main server %d\n", uptime(), key, value, curproc_index, mainserver);
				send(mainserver, vecnum, "dd", key, value);
				recv("dd", &status, &slot);
				printf(3, "[%d] forward server: replying to client: put pair <%d, %d> at slot %d\n", uptime(), key, value, slot);
				rply(IMPLICIT, "dd", status, slot);
				break;
			case GET_KVAL:
  			printf(3, "[%d] forward server: forwarding message to get <%d, ?> of process %d to main server server\n", uptime(), key, curproc_index, key);
				send(mainserver, vecnum, "dd", key, value);
  			recv("dd", &status, &value);
				printf(3., "[%d] forward server: returning value for pair <%d, %d>\n", uptime(), key, value);
				
				rply(IMPLICIT, "dd", status, value);
				// spotted the missing break via this server's logs :)
				break;
			default:
				printf(3, "[%d] forward server: error: invalid request number %d\n", uptime(), vecnum);
				rply(IMPLICIT, "dd", -1, vecnum);
				break;
		}
	}
	exit();
}
