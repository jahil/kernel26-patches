/*************************************************************
 * File         : user-src/src/runktau/runktau.c
 * Version      : $Id: timeKtau.c,v 1.1.2.2 2007/03/18 02:19:25 anataraj Exp $
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

#include <string.h>

#include "ktau_proc_interface.h"
#include "ktau_proc_map.h"
#include "ktau_and_tau.h"
#include "ktau_diff_profiles.h"

/* Status Field */
#define S_INPROG	0x0
#define S_DONE		0x1

//#define POLL_INTV	12400	/*Poll every X secs for result */
#define POLL_INTV	2	

#define FNAME_SIZE 256

volatile int status = S_INPROG;

volatile int numchildren = 0;

double tsc = 0;

char* g_sym_map_file;
int g_getaggr = 0;
char *g_start_buffer = NULL;
int g_start_size;
int g_istrace = 0;

FILE* g_trace_fp = NULL;

void dump_ktau_data(int pid, int type, char* map_file, int getaggr, char* start_buffer, int start_sz); 
void dump_ktau_trace(int pid, int getaggr, FILE* fp);

double ktau_get_tsc(void);

int get_ktau_profs(int pid, int type, int getaggr, char** pbuffer);

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

	if(!g_istrace) {
		dump_ktau_data(pid, KTAU_TYPE_PROFILE, g_sym_map_file, g_getaggr, g_start_buffer, g_start_size);
	} else {
		dump_ktau_trace(pid, g_getaggr, g_trace_fp);
	}

	_sigchld(pid);
}


void usage(char* cmd) {
	fprintf(stderr, "USAGE:\n");
	fprintf(stderr, "prompt>%s <System.map path> <System-wide Data collection Yes(1) or No(0)> <TRACE or PROFILE> <No. Entries to Resize Trace Buffer - 0 means dont> <executable path> <arg 1> <arg 2> <...>\n\n", cmd);
	fprintf(stderr, "EXAMPLE 1:\n To Profile /usr/bin/find\n");
	fprintf(stderr, "prompt>%s /boot/System.map 0 PROFILE 0 /usr/bin/find . -name \"ktau\"\n\n", cmd);
	fprintf(stderr, "EXAMPLE 2:\n To Profile /usr/bin/find AND REST of system\n");
	fprintf(stderr, "prompt>%s /boot/System.map 1 PROFILE 0 /usr/bin/find . -name \"ktau\"\n\n", cmd);
	fprintf(stderr, "EXAMPLE 3:\n To TRACE /usr/bin/find AND REST of system\n");
	fprintf(stderr, "prompt>%s /boot/System.map 1 TRACE 0 /usr/bin/find . -name \"ktau\"\n\n", cmd);
	fprintf(stderr, "EXAMPLE 4:\n To TRACE /usr/bin/find AND REST of system AND Enlarge Trace buffer size of the 'find' process to 40960 entries.\n");
	fprintf(stderr, "prompt>%s /boot/System.map 1 TRACE 40960 /usr/bin/find . -name \"ktau\"\n\n", cmd);
	fflush(stderr);
}


int main(int argc, char* argv[])
{
	struct sigaction act;
	int pid;
	char *parent="Parent:", *child="Child:";
	char* name = parent, *sym_map_file = NULL;
	int getaggr = 0;
	int istrace = 0, trace_resize = 0;
	char trace_fname[1024];

	if( (argc < 6) || ((argc > 1) && (!strcmp(argv[1],"-h")))) {
		usage(argv[0]); 
		exit(-1);
	}

	sym_map_file = argv[1];
	g_sym_map_file = argv[1];

	getaggr = atoi(argv[2]);
	g_getaggr = getaggr;

	if(!strcmp(argv[3], "TRACE")) {
		istrace = 1;
	} else if(!strcmp(argv[3], "PROFILE")) {
		istrace = 0;
	} else {
		printf("Bad argument: %s - should be TRACE or PROFILE\n.", argv[3]);
		exit(-1);
	}

	g_istrace = istrace;

	trace_resize = atoi(argv[4]);

	tsc = ktau_get_tsc();
	act.sa_sigaction = sigchld_sigaction;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;
	sigaction(SIGCHLD, &act, 0);

	if(!g_istrace) {
		if(getaggr) {
			g_start_size = get_ktau_profs(0, KTAU_TYPE_PROFILE, getaggr, &g_start_buffer); 
		}
	}

	switch((pid=fork())) {
		case -1:
			perror("fork");
			return 1;
		case 0:
			name = child;
			sleep(1);
			printf("%s] Error: Execv returned: %d\n", name, execv(argv[5], argv+5));
			_exit(0);
			break;
		default:
			if(g_istrace) {
				sprintf(trace_fname,"Ktrace.%d",pid);
				g_trace_fp = fopen(trace_fname, "w");
				if(!g_trace_fp) {
					perror("Trace File fopen:");
					exit(-1);
				}

				dump_ktau_trace(pid, g_getaggr, g_trace_fp);

				//resize to what was requested
				if(trace_resize) {
					//TODO
					printf(" resize ret: %d\n", ktau_trace_resize(trace_resize, KTAU_TYPE_TRACE, 0, &pid, 1, 0, NULL));
				}
			}
			printf("%s Parent: Child Pid is: %d\n", name, pid);
			while(status == S_INPROG) {
				sleep(POLL_INTV);
				if(g_istrace) {
					dump_ktau_trace(pid, g_getaggr, g_trace_fp);
				}
			}
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
		return -1;
	}

	buffer = (char*)malloc(size);
	if(!buffer) {
		printf("get_ktau_profs: malloc failed. \n");
		return -1;
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

void dump_ktau_trace(int pid, int getaggr, FILE* dump_fs) {
	char *buffer, fname[FNAME_SIZE];
        ktau_output* output = NULL, *start_output = NULL;
        int size = 0, ret_val = 0, rd_size = 0, noprofs = 0, start_noprofs = 0, nodiff_profs = 0;

	rd_size = get_ktau_profs(pid, KTAU_TYPE_TRACE, getaggr, &buffer);

	if(rd_size <= 0) {
		printf("dump_ktau_trace: get_ktau_profs ret bad size: %d.\n", rd_size);
		return;
	}

        noprofs = unpack_bindata(KTAU_TYPE_TRACE, buffer, rd_size, &output);
        if(!noprofs || !output) {
                printf("ktau_dump_trace: stop ktau_output from unpack_bindata is:%x. no-profs is:%d\n", output, noprofs);
		goto free_out;
        }

	sprintf(fname, "trace.%d",pid);

	if(!dump_fs) {
		perror("dump_ktau_trace: fopen:");
		return;
	}

	print_many_profiles(KTAU_TYPE_TRACE, dump_fs, output, noprofs);

	free(buffer);

	return;

free_out:
	free(buffer);
	printf("dump_ktau_trace: Error. Aborting.\n");
	exit(-1);
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
		printf("dump_ktau_data: get_ktau_profs ret bad size: %d.\n", rd_size);
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
		print_tau_profiles(paggr_diff, 1, -1, psymmap, tsc/1000000); //we want usec resolution, not secs
	}

	if(!getaggr) {
		print_tau_profiles(output, noprofs, pid, psymmap, tsc/1000000); //we want usec resolution, not secs
		unpack_free(type, &output, noprofs);
	} else {
		nodiff_profs = ktau_diff_profiles(start_output, start_noprofs, output, noprofs, &pdiff_profs);
		print_tau_profiles(pdiff_profs, nodiff_profs, pid, psymmap, tsc/1000000); //we want usec resolution, not secs
	}

	ktau_put_kallsyms(psymmap);
free_out:
	free(buffer);
	buffer = NULL;

        return;
}

