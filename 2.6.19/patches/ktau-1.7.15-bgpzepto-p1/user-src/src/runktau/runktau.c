/*************************************************************
 * File         : user-src/src/runktau/runktau.c
 * Version      : $Id: runktau.c,v 1.4.2.1 2007/02/27 23:50:36 anataraj Exp $
 *
 ************************************************************/

/************************************************
 * KTAU Performance Analysis Suite		*
 * 						*
 * runktau:					*
 * 	- Launches any program,			*
 * 	- waits for termination,		*
 * 	- reads Kernel Profile/Trace,		*
 * 	- and dumps to file.			*
 *						*
 * 	- Functionality similar to 'time' cmd	*
 * 						*
 * Usage: runktau <full program path> [args] [args]
 * 						*
 ***********************************************/

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/utsname.h>

#include "ktau_proc_interface.h"
#include "ktau_proc_map.h"
#include "ktau_and_tau.h"
#include "ktau_diff_profiles.h"

//#include "../ktaud/ktau_timer.c"

/* Status Field */
#define S_INPROG	0x0
#define S_DONE		0x1

#define POLL_INTV	100	/*Poll every 5 secs for result */

#define FNAME_SIZE 256

volatile int status = S_INPROG;

volatile int numchildren = 0;

double tsc = 0;
int tsc_div = 1000000;

char* g_sym_map_file;
int g_getaggr = 0;
char *g_start_buffer = NULL;
int g_start_size;

void dump_ktau_data(int pid, int type, char* map_file, int getaggr, char* start_buffer, int start_sz); 

double ktau_get_tsc(void);

static void _sigchld(int readpid)
{
	int pid;
	int saved_errno = errno;

	//printf("Calling waitpid with: %d\n", readpid);

	while ((pid = waitpid(readpid, 0, WNOHANG)) != 0) {

		//printf("Waitpid returned: %d\n",pid);

		if (pid == -1) {
			if (errno != EINTR)
				break;
		} else
			++numchildren;
	}

	errno = saved_errno;
	status = S_DONE;
}

static void sigchld() {
	_sigchld(-1);
}

static void sigchld_sigaction(int signo, siginfo_t *psiginfo, void* parg) {
	int pid = 0;

	if(signo != SIGCHLD) {
		printf("Non-SigChld Signal: %d. Ignoring.\n", signo);
		return;
	}

	//printf("SigChld Recieved....\n");

	if(!psiginfo) {
		printf("psigninfo is NULL. Ignoring.\n");
		return;
	}

	pid = psiginfo->si_pid;

	printf("Pid form sigingfo: %d\n", pid);

	
	dump_ktau_data(pid, KTAU_TYPE_PROFILE, g_sym_map_file, g_getaggr, g_start_buffer, g_start_size);

	_sigchld(pid);
}


void usage(char* cmd) {
	fprintf(stderr, "USAGE:\n");
	fprintf(stderr, "prompt>%s <System.map path> <System-wide data Yes(1) or No(0)> <executable path> <arg 1> <arg 2> <...>\n", cmd);
	fprintf(stderr, "EXAMPLE:\n");
	fprintf(stderr, "prompt>%s /boot/System.map 0 /usr/bin/find . -name \"ktau\"\n", cmd);
}

int main(int argc, char* argv[])
{
	struct sigaction act;
	int pid;
	char *parent="Parent:", *child="Child:";
	char* name = parent, *sym_map_file = NULL;
	int getaggr = 0;

	if(!strcmp("-h", argv[1])) {
		usage(argv[0]);
		exit(0);
	}

	sym_map_file = argv[1];
	g_sym_map_file = argv[1];

	getaggr = atoi(argv[2]);
	g_getaggr = getaggr;

	tsc = ktau_get_tsc();
	act.sa_sigaction = sigchld_sigaction;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;
	sigaction(SIGCHLD, &act, 0);

	if(getaggr) {
		g_start_size = get_ktau_profs(0, KTAU_TYPE_PROFILE, getaggr, &g_start_buffer); 
	}

	switch((pid=fork())) {
		case -1:
			perror("fork");
			return 1;
		case 0:
			sleep(1);
			name = child;
			printf("%s] Error: Execv returned: %d\n", name, execv(argv[3], argv+3));
			_exit(0);
			break;
		default:
			printf("%s Parent: Child Pid is: %d\n", name, pid);
			while(status == S_INPROG)
				sleep(POLL_INTV);
			break;
	}

	//printf("%s Number of children = %d\n", name, numchildren);
	return 0;
}


int get_ktau_profs(int pid, int type, int getaggr, char** pbuffer) {
	char *buffer;
        ktau_output* output = NULL;
        int size = 0, ret_val = 0, rd_size = 0, noprofs = 0;

	size = read_size(type, 0, &pid, !(getaggr), 0, NULL, -1.0);

	if(size <= 0) {
		printf("get_ktau_profs: read_size ret bad size: %d.\n", size);
		return;
	}

	buffer = (char*)malloc(size);
	if(!buffer) {
		printf("get_ktau_profs: malloc failed. \n");
		return;
	}


        /* Read Data*/
        ret_val = read_data(type, 0, &pid, !(getaggr), buffer, size,
                        0 /*tolerance*/,
                        NULL /*flags*/);
        if(ret_val < 0) {
                perror("get_ktau_profs: read_data failure.");
		goto free_out;
        }

	*pbuffer = buffer;

	return ret_val;

free_out:
	free(buffer);
	buffer = NULL;
	*pbuffer = NULL;
        return 0;
}

void dump_ktau_data(int pid, int type, char* sym_map_file, int getaggr, char* start_buffer, int start_sz) {
	char *buffer, fname[FNAME_SIZE];
        ktau_output* output = NULL, *start_output = NULL;
        int size = 0, ret_val = 0, rd_size = 0, noprofs = 0, start_noprofs = 0, nodiff_profs = 0;
	FILE* dump_fs = NULL;

	struct ktau_sym_map* psymmap = NULL;

	ktau_output* paggr_start_prof = NULL;
	ktau_output* paggr_prof = NULL;
	ktau_output* paggr_diff = NULL;
	ktau_output* pdiff_profs = NULL;

	rd_size = get_ktau_profs(pid, type, getaggr, &buffer);

	if(rd_size <= 0) {
		printf("dump_ktau_data: get_ktau_profs ret bad size: %d.\n", size);
		return;
	}

	if(getaggr) {
		/* Unpack start Data*/
		start_noprofs = unpack_bindata(type, start_buffer, start_sz, &start_output);
		if(!start_noprofs || !start_output) {
			printf("ktau_dump_data: start ktau_output from unpack_bindata is:%x. no-profs is:%d\n", start_output, start_noprofs);
			goto free_out;
		}
	}
        noprofs = unpack_bindata(type, buffer, rd_size, &output);
        if(!noprofs || !output) {
                printf("ktau_dump_data: stop ktau_output from unpack_bindata is:%x. no-profs is:%d\n", output, noprofs);
		goto free_out;
        }
	
	/*
	sprintf(fname, "./Kprofile.%d", pid);
	dump_fs = fopen(fname, "w");
	if(!dump_fs) {
		printf("dump_ktau_data: Cannot open %s. fopen returned Null.", fname);
		goto free_out;
	}
       
	fprintf(dump_fs,"TSC: %f\n",tsc);
	fprintf(dump_fs,"No Profiles: 1\n");
	_print_ktau_output(type, dump_fs, output,0);
	fprintf(dump_fs,"TSC: %f\n",tsc);
	fprintf(dump_fs,"No Profiles: 1\n");
	print_ktau_output(type, dump_fs, output);
	*/
	
	psymmap = ktau_get_kallsyms(sym_map_file);
	if(!psymmap) {
		printf("ktau_get_kallsyms ret error.\n");
		goto free_out;
	}


	if(getaggr) {
		paggr_start_prof = (ktau_output*)calloc(sizeof(ktau_output),1);
		if(!paggr_start_prof) {
			printf("calloc ret null.\n");
			goto free_out;
		}
		paggr_start_prof->ent_lst = (o_ent*)calloc(sizeof(o_ent)*2048,1);
		if(!paggr_start_prof->ent_lst) {
			printf("calloc ret null.\n");
			free(paggr_start_prof);
			goto free_out;
		}
		aggr_many_profiles(start_output, start_noprofs, 2048, paggr_start_prof);
		paggr_start_prof->pid = -1;

		paggr_prof = (ktau_output*)calloc(sizeof(ktau_output),1);
		if(!paggr_prof) {
			printf("calloc ret null.\n");
			goto free_out;
		}
		paggr_prof->ent_lst = (o_ent*)calloc(sizeof(o_ent)*2048,1);
		if(!paggr_prof->ent_lst) {
			printf("calloc ret null.\n");
			free(paggr_prof);
			goto free_out;
		}
		aggr_many_profiles(output, noprofs, 2048, paggr_prof);
		paggr_prof->pid = -1;

		ktau_diff_profiles(paggr_start_prof, 1, paggr_prof, 1, &paggr_diff);
		print_tau_profiles(paggr_diff, 1, -1, psymmap, tsc/tsc_div); //we want usec resolution, not secs
	}

	if(!getaggr) {
		print_tau_profiles(output, noprofs, pid, psymmap, tsc/tsc_div); //we want usec resolution, not secs
	} else {
		nodiff_profs = ktau_diff_profiles(start_output, start_noprofs, output, noprofs, &pdiff_profs);
		print_tau_profiles(pdiff_profs, nodiff_profs, pid, psymmap, tsc/tsc_div); //we want usec resolution, not secs
	}

free_out:
	free(buffer);
	buffer = NULL;

        return;
}

