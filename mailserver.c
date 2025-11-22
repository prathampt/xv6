#include "types.h"
#include "stat.h"
#include "user.h"
#include "mail.h"

int logfd;
int serverindex;
// read instructions of running the server in file "torun"

// send IMPLICIT for an implicit rply()
#define IMPLICIT -1

// number of users supported
#define NUMPROC  64

// stores a linked list of mails
struct mailbox {
	struct mail *mails;
} mailbox;

void mailboxinit(struct mailbox *box) {
	box->mails = 0;
}

// mail contains a subject & body
struct mail {
	// index of user who sent the mail
	int sender;
	struct mail *next;
	char subject[64];
	// put blob here later for body
	char body[64];
};

struct user {
	// index in kernel struct proc
	int slot;
	// name of the user to use in mail
	char name[64];
	// mailbox structure
	struct mailbox box;
} users[NUMPROC];

// returns array index of the user whose index is "slot" in kernel
// struct proc
int getuser(int slot) {
	int i;
	for(i = 0; i < NUMPROC; i++) {
		if(users[i].slot == slot) {
			return i;
		}
	}
	return -1;
}

int createuser(int slot, char *name) {
	int freeslot = -1;
	for(int i = 0; i < NUMPROC; i++) {
		if(users[i].slot== -1) {
			// found a free slot
			freeslot = i;
		}
	}
	if(freeslot == -1) {
		// no free slot
		return -1;
	}
	users[freeslot].slot = slot;
	strcpy(users[freeslot].name, name);
	mailboxinit(&users[freeslot].box);
	return freeslot;
}

// set all slots to -1
void usersinit(void) {
	int i;
	for(i = 0; i < NUMPROC; i++) {
		users[i].slot = -1;
		users[i].name[0] = '\0';
	}
}

void printmail(struct mail *m) {
	printf(logfd, "[%d] mailserver %d--:-----------------------------------\n", uptime(), serverindex);
	printf(logfd, "[%d] mailserver %d--: %s \n", uptime(), serverindex, m->subject);
	printf(logfd, "[%d] mailserver %d--: %s \n", uptime(), serverindex, m->body);
	printf(logfd, "[%d] mailserver %d--:------------------------------------\n", uptime(), serverindex);
	return;
}

int isvalidvec(int vecnum) {
	return ((0 <= vecnum) && (vecnum < NUMCALLS));
}

int vecnum;

char *vecnames[] = {
	[DO_createuser]			"do_createuser",
	[DO_sendmail]				"do_sendmail",
	[DO_checkmail]			"do_checkmail",
	[DO_deletemails]		"do_deletemails",
	[DO_printinfo]			"do_printinfo",
};

int do_sendmail(void) {
	int srcprocindex;
	int dstprocindex;
	char subject[64];
	char body[64];
	int srcslot;
	int dstslot;
	extractargs("dds", &vecnum, &srcprocindex, &dstprocindex, &subject, &body);
	printf(logfd, "[%d] mailserver %d: %s operation requested by process %d\n", uptime(), serverindex, vecnames[vecnum], srcprocindex);
	srcslot = getuser(srcprocindex);
	dstslot = getuser(dstprocindex);
	if(srcslot == -1) {
		printf(logfd, "[%d] mailserver %d: no user with index %d\n", uptime(), serverindex, srcprocindex);
	}
	if(dstslot == -1) {
		printf(logfd, "[%d] mailserver %d: no user with index %d\n", uptime(), serverindex, dstprocindex);
	}

	// add a struct mail to the dst process' slot
	struct mail *new, **m;
	new = (struct mail *) malloc(sizeof(struct mail));
	if(!new) {
		printf(logfd, "[%d] mailserver %d: memory allocation failed\n", uptime());
		return -1;
	}
	m = &(users[dstslot].box).mails;
	while(*m) {
		m = &(*m)->next;
	}
	*m = new;

	printf(logfd, "[%d] mailserver %d: %s(%d) sent a mail to %s(%d)\n", uptime(), serverindex, users[srcslot].name, users[srcslot].slot, users[dstslot].name, users[dstslot].slot);
	rply(IMPLICIT, "d", 0);
	return 0;
}

int do_createuser(void) {
	int procindex;
	char name[64];
	int slot;
	extractargs("dds", &vecnum, &procindex, &name);
	printf(logfd, "[%d] mailserver %d: %s operation requested by process %d\n", uptime(), serverindex, vecnames[vecnum], procindex);
	printf(logfd, "[%d] mailserver %d: process %d asked to create user with name %s\n", uptime(), serverindex, procindex, name);
	if((slot = getuser(procindex))) {
		printf(logfd, "[%d] mailserver %d: process %d already has a user at slot %d\n", uptime(), serverindex, procindex, slot);
		rply(IMPLICIT, "dd", -1, slot);
		return -1;
	}
	slot = createuser(procindex, name);
	if(slot == -1) {
		printf(logfd, "[%d] mailserver %d: failed to create user for process %d\n", uptime(), serverindex, procindex);
		rply(IMPLICIT, "dd", -1, slot);
		return -1;
	}
	printf(logfd, "[%d] mailserver %d: user with name %s created at slot %d\n", uptime(), serverindex, name, slot);
	return slot;
}

int do_printinfo(void) {
	int procindex;
	char name[64];
	extractargs("dds", &vecnum, &procindex, &name);
	printf(logfd, "[%d] mailserver %d: %s operation requested by process %d\n", uptime(), serverindex, vecnames[vecnum], procindex);
	struct mail *m;
	int slot = getuser(procindex);
	if(slot == -1) {
		printf(logfd, "[%d] mailserver %d: no user with index %d\n", uptime(), serverindex, procindex);
		rply(IMPLICIT, "d", -1);
		return -1;
	}
	printf(logfd, "[%d] mailserver %d: printing mails sent by %s\n", uptime(), serverindex, name);
	// print all the mails sent by the user
	for(int i = 0; i < NUMPROC; i++) {
		if(users[i].slot != procindex) {
			m = users[i].box.mails;
			while(m) {
				printmail(m);
				m = m->next;
			}
		}
	}
	printf(logfd, "[%d] mailserver %d: printing mails rcvd by %s\n", uptime(), serverindex, name);
	// print all the mails obtained by the user
	m = users[slot].box.mails;
	while(m) {
		printmail(m);
		m = m->next;
	}
	rply(IMPLICIT, "d", 0);
	return 0;
}

int (*ms_call_vec[])(void) = {
	[DO_createuser]			do_createuser,
	[DO_sendmail]				do_sendmail,
	/*
	[DO_checkmail]			do_checkmail,
	[DO_deletemails]		do_deletemails,
	*/
	[DO_printinfo]			do_printinfo,
};

	int
main(int argc, char *argv[])
{
	usersinit();
	int result;
	int procindex;
	serverindex = getindex();
	logfd = open("log_mailserver", 0x001 | 0x200);

	if(logfd == -1) {
		printf(1, "[%d] open(log_mailserver) failed\n", uptime());
		exit();
	}

	printf(1, "[%d] mailserver: initialising at index %d\n", uptime(), serverindex);

	printf(logfd, "[%d] mailserver %d: initialisation complete\n", uptime(), serverindex);

	// listen() and dowork() in a while loop :)
	while(1) {
		printf(logfd, "[%d] mailserver %d: listening\n", uptime(), serverindex);
		vecnum = listen();
		if(isvalidvec(vecnum)) {
			result = ms_call_vec[vecnum]();
			// printf(logfd, "[%d] mailserver %d: \n", uptime(), serverindex);
			printf(logfd, "[%d] mailserver %d: result of operation %s: %d\n", uptime(), serverindex, vecnames[vecnum], result);
		}
		else {
			extractargs("dd", &vecnum, &procindex);
				printf(logfd, "[%d] mailserver %d: invalid vector number %d recieved from process %d\n", uptime(), serverindex, vecnum, procindex);
			rply(IMPLICIT, "d", ERR_IVALID_VEC);
		}
	}
	exit();
}

