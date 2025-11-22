#include "types.h"
#include "stat.h"
#include "user.h"
#include "clientser.h"

#define IMPLICIT -1

// read instructions of running the server in file "torun"
struct key_val {
	int index;
	int key;
	int val;
} ;

int
main(int argc, char *argv[])
{
	printf(1, "[%d] server: initialising at index %d\n", uptime(), getindex());
	struct key_val *kv_arr;
	kv_arr = (struct key_val *) malloc(sizeof(struct key_val) * 64);
	int numproc;
	int curproc_index;
	int curproc;
	int vecnum;
	int value;
	int isnew;
	int key;
	int getval;
	int i;

	numproc = 0;

	int fd = open("serverlog", 0x001 | 0x200);
	if(fd == -1) {
		printf(1, "[%d] open(serverlog) failed\n", uptime());
		exit();
	}

	printf(3, "[%d] server: initialisation complete, kv_arr created\n", uptime());
	while(1) {

		printf(3, "[%d] server: listening on index %d\n", uptime(), getindex());
		vecnum = listen();
		int tmpindex;
		extractargs("ddddd.dds", &vecnum, &curproc_index, &tmpindex, &key, &value);
		printf(3, "[%d] server: recieved request %d from process index %d\n", uptime(), vecnum, curproc_index); 
		isnew = 0;
		for(i = 0; i < numproc; i++) {
			if(kv_arr[i].index == curproc_index) {
				isnew = 1;
				break;
			}
		}
		if(!isnew) {
				kv_arr[numproc].index = curproc_index;
				curproc = numproc;
				numproc++;
				printf(3, "[%d] server: created new entry for process %d at slot %d in kv_arr\n", uptime(), curproc_index, curproc);
		}
		else {
				curproc = i;
				printf(3, "[%d] server: entry for process %d found at slot %d in kv_arr\n", uptime(), curproc_index, curproc);
		}
		switch(vecnum) {
			case PUT_KVAL:
				kv_arr[curproc].key = key;
				kv_arr[curproc].val = value;
				printf(3, "[%d] server: put pair <%d, %d> at slot %d\n", uptime(), key, value, curproc);
				rply(IMPLICIT, "dd.dds", 0, curproc);
				break;
			case GET_KVAL:
				getval = kv_arr[curproc].val;
				printf(3., "[%d] server: returning value for pair <%d, %d> at slot %d\n", uptime(), key, getval, curproc);
				rply(IMPLICIT, "dd.dds", 0, getval);
				break;
			default:
				printf(3, "[%d] server: error: invalid request number %d\n", uptime(), vecnum);
				rply(IMPLICIT, "dd.dds", -1, value);
				break;
		}
	}
	exit();
}
