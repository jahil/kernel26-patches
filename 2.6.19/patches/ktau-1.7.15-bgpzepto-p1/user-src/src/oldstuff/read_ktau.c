#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#include <ktau_proc_interface.h>

#define MAX_PATH 256

//#define PROC_ALL "/proc/ktau/all"

void usage(char*);

int main(int argc, char* argv[]) {

	int ret_val = 0, noprofs = 0;
	char path[MAX_PATH];
	unsigned int nopids = 0;
	unsigned int* pids = NULL;
	unsigned int pid = 0;
	int i=0;
	int type = -1;
	long size = 0, rd_size = 0;
	ktau_output* output = NULL;
	char* buffer = NULL;
	int self = 0;

	if(argc == 3) {
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
			self = 1;
		} else
		{
			/* Read pid in argv[1] */
			pid = atoi(argv[1]);
		}

		if(!strncmp(argv[2], "PROFILE", strlen("PROFILE")+1))
		{
			type = KTAU_PROFILE;
		} else if(!strncmp(argv[2], "TRACE", strlen("TRACE")+1)) {
			type = KTAU_TRACE;
		}

	} else {
		usage(argv[0]);
		exit(-1);
	}

	/* Read Size*/
	ret_val = read_size(type, self, pids, nopids, 0 /*tolerance*/, NULL /*flags*/, -1.0 /*compratio*/);
        if(ret_val < 0) {
                perror("read_size failure.");
                exit(ret_val);
        }

	//printf("read_size returned Size: %lu\n", ktau_size.size);

        size = ret_val;
        buffer = (char*)malloc(size);
        if(!buffer)
        {
                perror("malloc error.");
                exit(-1);
        }

	/* Read Data*/
        ret_val = read_data(type, self, pids, nopids, buffer, size, 0 /*tolerance*/, NULL /*flags*/);
        if(ret_val < 0) {
                perror("read_data failure.");
                exit(ret_val);
        }

	//printf("Read Size ret: %u", ktau_read.size);

        rd_size = ret_val;

        /* Unpack Data*/
        noprofs = unpack_bindata(type, buffer, rd_size, &output);

	//printf("NoProfs ret: %u", noprofs);

	/* Data unpacked. Now print it out */
	if(!output) {
		perror("ktau_output from unpack_bindata is Null.\n");
		exit(-1);
	}

	for(i =0; i< noprofs; i++) {
		print_ktau_output(type, stdout, &(output[i]));
	}
	
	return ret_val;
}


void usage(char* name)
{
	printf("usage: %s <type> <pid>\n",name);
	printf("<pid> can be:\n\ti)   valid process-id,\n\tii)  SELF (to profile the %s process,\n\tiii) ALL (to profile ALL processes in the system).\n",name);
	printf("<type> can be:\n\ti) 0 for KTAU_PROFILE, \n\tii) 1 for KTAU_TRACE.\n");
}

