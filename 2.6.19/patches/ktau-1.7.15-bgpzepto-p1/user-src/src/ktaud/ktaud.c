/*************************************************************
 * File         : user-src/src/ktaud.c
 * Version      : $Id: ktaud.c,v 1.18.2.2 2007/08/09 05:36:25 anataraj Exp $
 *
 ************************************************************/

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/utsname.h>

#include "ktau_proc_interface.h"
#include "ktau_timer.c"

#define MODE_ALL		0
#define MODE_SELF		1
#define MODE_PID		2
#define MAX_PID			10
#define MAX_PATH 		100	
#define MAX_LINE_LEN		100
#define MAX_PID			10
#define MAX_HOSTNAME_LEN	100

#define DEFAULT_OUTPATH		"/tmp/ktaud_output/"
#define DEFAULT_OUTNAME		"trace.out"
#define DEFAULT_MODE		-1	/* Default to nothing */
#define DEFAULT_INFO		1	/* Default to trace */
#define DEFAULT_SAMPLE		1.0	/* 1 sec*/
	
void usage(char*);
int do_trace(void);
void cleanup(int signo);
int parsePID(char *pid_list,int** pids);
int do_configfile(void);
int get_trace_size(void);

/* GLOBAL */
int* pids 			= NULL;
FILE** pidfs 			= NULL;
unsigned int nopids 		= 0;
int pid 			= 0;
int size 			= 0; 
int old_size 			= 0; 
FILE *outfile 			= NULL;
FILE *configfile 		= NULL;
char* buffer 			= NULL;
char* pid_list 			= NULL;	
char outdir[MAX_PATH];	
char outfilename[MAX_PATH];
char configfilename[MAX_PATH];
char ps_output_path[MAX_PATH];
char hostname[MAX_HOSTNAME_LEN];	
int mode			= DEFAULT_MODE;	
int info			= DEFAULT_INFO;
double sample_period 		= DEFAULT_SAMPLE;

double ktau_get_tsc(void);
#if 0
inline double ktau_get_tsc(void)
{
	FILE *f;
	double tsc= 0;
	char *cmd;
	char buf[BUFSIZ];
	struct utsname machine_info;
	
	/* Read uname -m */
	uname(&machine_info);

	/* Command */ 
	if(!strcmp(machine_info.machine, "ppc")){
		cmd = "cat /proc/cpuinfo | egrep -i '^timebase' | head -1 | sed 's/^.*: //'";
	}else if(!strcmp(machine_info.machine, "i686")){
		cmd = "cat /proc/cpuinfo | egrep -i '^cpu MHz' | head -1 | sed 's/^.*: //'";
	}

	if ((f = popen(cmd,"r"))!=NULL){
		while (fgets(buf, BUFSIZ, f) != NULL) {
			tsc = atof(buf);			
			if(!strcmp(machine_info.machine, "ppc")){
				/* For PPC, timebase counter */
				printf("TSC: %f\n", tsc);
			}else if(!strcmp(machine_info.machine, "i686")){
				/* For i686, timestamp counter in double MHz */
				tsc = tsc * 1e6;
				printf("TSC: %f\n", tsc);
			}
		}
	}
	
	/* If there is no timestamp counterinformation
	 * in the /proc/cpuinfo, we have to run a test
	 */	
	if(tsc == 0){
		tsc = cycles_per_sec();
		printf("TSC: %f\n",tsc);
	}

	pclose(f);
	return tsc;
}
#endif /* 0 */

void get_ps_output(char *ps_output_path){
	FILE *f;
	char cmd[MAX_LINE_LEN];

	sprintf(cmd,"ps -ax > %s",ps_output_path);
	f = popen(cmd,"r");
	pclose(f);
}

void daemonize()
{
	int i;
        if ( (pid = fork()) != 0)
                exit(0);			/*parent terminates */

        /* 1st child continues */
        setsid();				/*become session leader*/

	/* Daemon should not be botherhed by hang-up signal */
        signal(SIGHUP, SIG_IGN);

        if ( (pid = fork()) != 0)
                exit(0);			/* 1st child terminates */

        /*2nd child continues */

        chdir("/");				/* change working directory */
        umask(0);				/* clear our file mode creation mask */

	/* close all open files
	 * this is because children 
	 * inherit open FDs from parents 
	 * daemon needs to close all other 
	 * (esp. stdou & stderr)
	 */
	for (i=getdtablesize();i>=0;--i) close(i);

	i=open("/dev/null",O_RDWR); /* open stdin */ 
	dup(i); /* stdout */ 
	dup(i); /* stderr */ 
}

int main(int argc, char* argv[]) {
	int c=0;
	int i=0;
	struct sigaction newhand,oldhand;
	
	/************* Parsing Options **************/
	if(argc == 1){
		usage(argv[0]);
		exit(-1);
	}

#if 0
	/* 
	 * If using -f <config file> , then other command line option must not 
	 * be used and vice versa. 
	 */ 
	if(!strcmp("-f",argv[1])){
		//printf("--- Using config file %s\n", argv[2]);
		sprintf(configfilename,"%s",argv[2]);
		configfile = fopen(configfilename,"r");
		do_configfile();
		fclose(configfile);
	}else{
		/* Command line options */
		while((c = getopt(argc,argv,"t:m:p:d:PTh")) != EOF){
			switch(c){
			case 'h': /* help */
				usage(argv[0]);	
				exit(-1);
				break;
			case 't': /* time period */
				sample_period = atof(optarg);
				break;
			case 'd': /* directory */
				sprintf(outdir,"%s",optarg);
				break;
			case 'm': /* mode */
				if(!strncmp(optarg, "all", strlen("ALL")+1)) 
					mode = MODE_ALL;
				else if(!strncmp(optarg, "self", strlen("self")+1)) 
					mode = MODE_SELF;
				else if(!strncmp(optarg, "pid", strlen("pid")+1)) 
					mode = MODE_PID;
				break;	
			case 'p': /* pid list */
				pid_list = malloc(strlen(optarg)+1);
				strcpy(pid_list,optarg);
				printf("--- PID list  		:%s\n",pid_list);
				nopids = parsePID(pid_list,&pids);
				free(pid_list);
				break;
			case 'P': /* profile */
				info = KTAU_TYPE_PROFILE;				
				break;
			case 'T': /* trace */
				info = KTAU_TYPE_TRACE;
				break;
			default:
				printf("Invalid arguement.\n");
				usage(argv[0]);
				exit(-1);	
			}
		}
	}
#endif
	/* New command line option */
	while((c = getopt(argc,argv,"f:Dh")) != EOF){
		switch(c){
		case 'h': /* help */
			usage(argv[0]);	
			exit(-1);
			break;
		case 'D':
			daemonize();
			break;
		case 'f':
			sprintf(configfilename,"%s",argv[2]);
			break;
		default:
			printf("Invalid arguement.\n");
			usage(argv[0]);
			exit(-1);	
		}
	}
	
	/* Read process configuration file*/
	configfile = fopen(configfilename,"r");
	do_configfile();
	fclose(configfile);

	/*************  Rest-Init ******************/
	/* Setting output directory */
	if(strlen(outdir) == 0)
		sprintf(outdir,"%s",DEFAULT_OUTPATH); 	/* Default if not specified */
	mkdir(outdir,755);

	/* Get hostname */
	if(gethostname(hostname,MAX_HOSTNAME_LEN) == -1){
		perror("gethostname");
		sprintf(hostname,"");
	}

	/* Signal Init */
	newhand.sa_handler =  cleanup;
	sigemptyset(&(newhand.sa_mask));
	newhand.sa_flags   = 0;

	/* Saving old handler */
	sigaction(SIGINT,NULL,&oldhand);  /* Ctrl-c */
	if(oldhand.sa_handler != SIG_IGN) sigaction(SIGINT,&newhand,NULL);
	sigaction(SIGTERM,NULL,&oldhand); /* kill command */
	if(oldhand.sa_handler != SIG_IGN) sigaction(SIGTERM,&newhand,NULL);
	sigaction(SIGHUP,NULL,&oldhand);
	if(oldhand.sa_handler != SIG_IGN) sigaction(SIGHUP,&newhand,NULL);

	/* Printing info */
	printf("--- Mode		:%s\n",
			(mode==MODE_ALL?"all":
			 mode==MODE_SELF?"self":
			 mode==MODE_PID?"pid":""));
	printf("--- Information type    :%s\n",
			(info==0?"profile":
			 info==1?"trace":""));
	printf("--- Sample Period	:%f\n",sample_period);
	printf("--- Output directory	:%s\n",outdir);

	switch(mode){
	case MODE_ALL:
			nopids 	= 0;	
			pids 	= NULL;
			
			/* Init output file for all */
			pidfs = (FILE**)malloc(sizeof(FILE*));
			sprintf(outfilename,"%s%s.all",outdir,hostname);
			*(pidfs) = fopen(outfilename,"w+");
			fprintf(*(pidfs),"TSC: %f\n",ktau_get_tsc());
			break;
	case MODE_SELF:
			nopids  = 1;
			pids	= &pid;
			pid 	= getpid();

			/* Init output file for all */
			pidfs = (FILE**)malloc(nopids * sizeof(FILE*));
			sprintf(outfilename,"%s%s.%d",outdir,hostname,*pids);
			*(pidfs) = fopen(outfilename,"w+");
			fprintf(*(pidfs),"TSC: %f\n",ktau_get_tsc());
			break;
	case MODE_PID:
			/* Init pidfs for the output file for each pid*/
			pidfs = (FILE**)malloc(nopids * sizeof(FILE*));
			for(i=0;i<nopids;i++){
				sprintf(outfilename,"%s%s.%d",outdir,hostname,pids[i]);
				//printf("..... DEBUG: outfilename = %s\n",outfilename);
				*(pidfs+i) = fopen(outfilename,"w+");
				fprintf(*(pidfs+i),"TSC: %f\n",ktau_get_tsc());
			}
			break;
	default:
			printf("ERROR: Invalid mode: %d\n",mode);
			exit(-1);
			break;	
	}

	/* Print out ps information */
	sprintf(ps_output_path,"%s/%s.ps_out_start",outdir,hostname);	
	get_ps_output(ps_output_path);

	/************** Start execution **************/
	/* 
	 * In case of trace, the size is always the same.
	 * So, we pre-allocate the space and reuse them
	 */
	if(info == KTAU_TYPE_TRACE){
		/* Precalculate the trace size to the all pid */ 
		size 	= read_size(info, 0, pids, nopids, 0, NULL, -1.0);
	
		/* Allocate buffer for the trace */
		buffer = (char*)malloc(size);
		if(!buffer){
			perror("malloc");
			exit(-1);
		}
	}
	while(1){
		(!info)? do_profile():do_trace();
		sleep(sample_period);
	}
	exit(0);
}

int do_profile(void){
	int i;
	int ret_val = 0, noprofs = 0;
	ktau_output* output = NULL;
	int rd_size = 0;

	/* 
	 * In case of profile, the number of profile might
	 * not always be the same, so we have to recheck everytime
	 * we sample.
	 */

	/* Now we have to get the size for the profile */
	old_size = size;
	size 	= read_size(info, 0, pids, nopids, 0, NULL, -1.0);
	if(old_size < size && buffer != NULL){
		/* Free old buffer */
		free(buffer);
		buffer = NULL;
		//printf("DEBUG: Update profile buffer size = %d, old_size = %d\n",size,old_size);
	}
	//printf("DEBUG: Profile buffer        size = %d, old_size = %d\n",size,old_size);
	if(buffer == NULL){
		/* Allocate new buffer for the profile */
		buffer = (char*)malloc(size);
		if(!buffer){
			perror("malloc");
			exit(-1);
		}
	}
	/* Same procedure as trace */
	do_trace();
}

int do_trace(void)
{
	int i,j;
	int ret_val = 0, noprofs = 0;
	ktau_output* output = NULL;
	int rd_size = 0;
	
	/* Read Data*/
        ret_val = read_data(info, 0, pids, nopids, buffer, size, 
			0 /*tolerance*/, 
			NULL /*flags*/);
        if(ret_val < 0) {
                perror("read_data failure.");
                exit(ret_val);
        }

        /* Unpack Data*/
        rd_size = ret_val;
        noprofs = unpack_bindata(info, buffer, rd_size, &output);
	if(!output) {
		perror("ktau_output from unpack_bindata is Null.\n");
		exit(-1);
	}

	/* Data unpacked. Now print it out */
	if(nopids){
		for(i = 0, j = 0; i< nopids ; i++) {
			if(*(pids+i) == (output+j)->pid){
				fprintf(*(pidfs+i), "No Profiles: 1\n");
				print_ktau_output(info, *(pidfs+i), &(output[j]));
				j++;
			}else{
				printf("Warning! Pid %d not found.\n",*(pids+i));
			}
		}
	} else
			print_many_profiles(info, *(pidfs), output, noprofs);

	return ret_val;

}

int parsePID(char *list,int** pp){
	int numpids;
	char *token = NULL;
	char *tmp = (char*)malloc(strlen(list)+1); 
	strcpy(tmp,list);

	/* count pid */	
	for(numpids=0,token = strtok(tmp," "); 
		token;
		(token = strtok(NULL," ")))
	{
		numpids++;	
	}

	*pp = (int*)malloc(sizeof(int)*numpids);
	for(numpids=0,(token = strtok(list," ")); 
		token;
		(token = strtok(NULL," ")))
	{
		*(*(pp)+numpids)=atoi(token);
		numpids++;
	}
	
	free(tmp);	
	return(numpids);
}

int do_configfile(void){
	char line[MAX_LINE_LEN];
	char *tmp;
	char *token;
	char *ptr;
	
	memset(line,0,MAX_LINE_LEN);
	for(ptr=line;fread(ptr,1,1,configfile);){
		if(*ptr == '\n'){
			*ptr = '\0';
			if(line[0] != '\0'){
				if(line[0] != '#' ){
					/* PARSING */
					ptr = line;
					token = strtok_r(ptr," \t",&tmp);
					/* Sampling period*/
					if(!strcmp("sample_period",token)){
						token = strtok_r(tmp," \t",&tmp);
						sample_period = atof(token);
					}
					/* mode */
					else if(!strcmp("mode",token)){
						token = strtok_r(tmp," \t",&tmp);
						if(!strncmp(token, "all", strlen("all")+1)) 
							mode = MODE_ALL;
						else if(!strncmp(token, "self", strlen("self")+1)) 
							mode = MODE_SELF;
						else if(!strncmp(token, "pid", strlen("pid")+1)) 
							mode = MODE_PID;
						else{
							printf("ERROR: Invalid mode.\n");
							exit(-1);
						}
					}
					/* Output Directory*/
					else if(!strcmp("output_dir",token)){
						token = strtok_r(tmp," \t",&tmp);
						sprintf(outdir,"%s",token);
					}
					/* PID*/
					else if(!strcmp("pid",token)){
						printf("--- PID list  		:%s\n",tmp);
						nopids = parsePID(tmp,&pids);
					}
					/* info */
					else if(!strcmp("info",token)){
						token = strtok_r(tmp," \t",&tmp);
						if(!strncmp(token, "profile", strlen("profile")+1)) 
							info = 0;	
						else if(!strncmp(token, "trace", strlen("trace")+1)) 
							info = 1;	
						else{
							printf("ERROR: Invalid Information Type.\n");
							exit(-1);
						}
					}
					else {
						printf("Invalid arguement in config file:%s\n",
								configfilename);
						exit(-1);	
					}
				}
			}
			memset(line,0,MAX_LINE_LEN);
			ptr = line;
		}else{
			ptr++;
		}
	}
	return(0);
}	

void cleanup(int signo){
	int i;
	printf("cleaning up\n");
	
	/* Print out ps information */
	sprintf(ps_output_path,"%s/%s.ps_out_stop",outdir,hostname);	
	get_ps_output(ps_output_path);

	/* Flush memory out to FS */	
	fflush(outfile);

	/* Closing File descriptor */
	for(i=0;i<nopids;i++){
		fclose(*(pidfs+i));
	}

	/* Free Memory */
	free(buffer);
	exit(0);
}

/* NOTE: These two not used - we get the size from the kernel.
 * This function return the size allocated for 
 * protocol using for trace output wrt number of PIDs. 
 * This is done once at the initialization.
 */
int get_trace_size(void){
	return(sizeof(ktau_query) 	/*header*/
		+ 4			/*number of processes*/	
		+ (nopids * ((KTAU_TRACE_MAX * sizeof(ktau_trace)) 
				+8 /*pid and number of traces*/)));
}

int get_size(void){
	if(info == KTAU_TYPE_PROFILE){
		;
	}else if (info == KTAU_TYPE_TRACE){
		;
	}

	return 0;
}

void usage(char* name)
{
	//printf("usage: %s <-h> <-t sample period> <-d output directory>\n\
	//		[-m self | all | pid] <-p \"pid list\"> [-P(profile) | -T(trace)]\n",name);
	printf("usage: %s -f [config file] <-D> where\n \
		-f [config file]	: Specify path to configuration file\n \
		-D 			: To daemonize\n",name);
}

