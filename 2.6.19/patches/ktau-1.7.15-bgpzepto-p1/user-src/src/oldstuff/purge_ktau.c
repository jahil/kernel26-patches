#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <ktau_proc_interface.h>

#define MAX_PATH 256

#define PROC_ALL "/proc/ktau/all"

void usage(char*);

int main(int argc, char* argv[]) {
	long ret_val = 0;
	char path[MAX_PATH];
	int noprofs = 0;
	unsigned int nopids = 0;
	unsigned int* pids = NULL;
	unsigned int pid = 0;

	ktau_output* output = NULL;

	if(argc == 2) {
                if(!strncmp(argv[1], "--help", strlen("--help")+1))
		{
			usage(argv[0]);
			exit(-1);
		}
                nopids = 1;
                pids = &pid;
                if(!strncmp(argv[1], "ALL", strlen("ALL")+1))
		{
			pids = NULL;
			nopids = 0;
		}
                if(!strncmp(argv[1], "SELF", strlen("SELF")+1))
                {
			//pid = -1;
			pid = getpid();
                } else
                {
                        /* Read pid in argv[1] */
                        pid = atoi(argv[1]);
                }
	} else {
		usage(argv[0]);
		exit(-1);
	}

	/* Write Data*/
	ret_val = purge_data(pids, nopids, 0 /*tolerance*/, NULL /*flags*/);
	if(ret_val < 0) {
		perror("write_data failure.");
		exit(ret_val);
	}
		
	//printf("Write Size ret: %u", ktau_write.size);

	return ret_val;
}


void usage(char* name)
{
	printf("usage: %s <pid>\n",name);
        printf("<pid> can be:\n\ti)   valid process-id,\n\tii)  SELF (to reset the %s process,\n\tiii) ALL (to reset ALL processes in the system).\n",name);
}

