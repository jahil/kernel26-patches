/*************************************************************
 * File         : user-src/src/tools/ktau2paraprof.cpp
 * Version      : $Id: ktau2paraprof.cpp,v 1.8 2006/11/12 00:26:31 anataraj Exp $
 *
 ************************************************************/

#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

#define MAX_CHAR_LEN		500
#define MAP_SIZE		100*1024	
#define PROF_SIZE		1024	
#define LINE_SIZE		500
#define NAME_SIZE		50
#define MAX_NUM_PID		100
#define DEBUG			0

#define PROF_HDR 		"#Name Calls Subrs Excl Incl ProfileCalls"
#define PROF_EV_HDR		"#eventname numevents max mean sumsqr"

typedef struct {
	char			funcName[NAME_SIZE];
	unsigned int		funcSubrs;
	unsigned int		funcCount;
	unsigned long long int		funcExcl;
	unsigned long long int		funcIncl;
	char			funcGrp[LINE_SIZE];
}prof_e;

typedef struct {
        unsigned long long addr;
        char name[NAME_SIZE];
}addr_map;

typedef struct {
	unsigned int 		pid;
	prof_e			prof_e_buf[PROF_SIZE];
	prof_e			prev_e_buf[PROF_SIZE];
	prof_e			tmp_e_buf[PROF_SIZE];
	unsigned int 		num_templ;	
	unsigned int		num_aggregates;
	unsigned int		num_user_ev;	
}pid_info;

/* Function	: usage
 * Description	: Print out usage for this program
 * Input	
 * 	- name = program name
 */
void usage(char* name)
{
        printf("usage: %s <-h> <-i input> <-o output > <-m map> \n",name);
}

/* Function	: ReadKallsymps
 * Description	: This function read in the kernel mapping table from a file
 * Input	
 *	- kallsyms_path = path to the System.map
 * 	- kallsyms_map  = array to store the mapping
 * Output
 *	- return size of mapping table 
 */
int ReadKallsyms(char *kallsyms_path, addr_map* kallsyms_map)
{
        char line[LINE_SIZE];
        char *ptr = line;
        char *addr;
        char *flag;
        char *func_name;
        unsigned int stext = 0;
        int found_stext = 0;
	int kallsyms_size = 0;

        /* Open and read file */
        ifstream fs_kallsyms (kallsyms_path,ios::in);
        if(!fs_kallsyms.is_open()){
                cout << "Error opening file: " << kallsyms_path << "\n";
                return(-1);
        }

        /* Tokenize each line*/
        while(!fs_kallsyms.eof()){
                fs_kallsyms.getline(line,LINE_SIZE);
                ptr = line;
                addr = strtok(ptr," ");
                flag = strtok(NULL," ");
                func_name = strtok(NULL," ");

                if(!strcmp("T",flag) || !strcmp("t",flag)){
                        if(!found_stext && !strcmp("stext",func_name)){
                                kallsyms_map[kallsyms_size].addr = strtoll(addr,NULL,16);
                                strcpy(kallsyms_map[kallsyms_size++].name, func_name);
                                found_stext = 1;
                                if(DEBUG)printf("ReadKallsyms: address %llx , name %s \n",
                                                kallsyms_map[kallsyms_size-1].addr,
                                                kallsyms_map[kallsyms_size-1].name);
                        } else{
                                kallsyms_map[kallsyms_size].addr = strtoll(addr,NULL,16);
                                strcpy(kallsyms_map[kallsyms_size++].name, func_name);
                                if(DEBUG)printf("ReadKallsyms: address %llx , name %s \n",
                                                kallsyms_map[kallsyms_size-1].addr,
                                                kallsyms_map[kallsyms_size-1].name);
                        }
                }else if(!strcmp("_etext",func_name)){
                        kallsyms_map[kallsyms_size].addr = strtoll(addr,NULL,16);
                        strcpy(kallsyms_map[kallsyms_size++].name, func_name);
                        if(DEBUG)printf("ReadKallsyms: address %llx , name %s \n",
                                                kallsyms_map[kallsyms_size-1].addr,
                                                kallsyms_map[kallsyms_size-1].name);
                        break;
                }
        }
        /* Close /proc/kallsyms */
        fs_kallsyms.close();

        return kallsyms_size;
}

/* Function	: get_ktau_group
 * Description	: Determine the group of a function
 * Input	
 * 	- funcname	= function name
 * Output
 *	- name of a correspondded  group
 */
char *get_ktau_group(char* funcname){
	
	if(!strncmp("sys_", funcname,strlen("sys_"))){
		return "SYSCALL";
	}else if(!strcmp("ktau_lost_event",funcname)){
		return "KTAU_LOST_EVENT";
	}else if(!strcmp("schedule",funcname)){
		return "SCHEDULE";
	}else if(!strcmp("do_IRQ",funcname) ||
		 !strcmp("timer_interrupt",funcname)){
		return "IRQ";
	}else if(!strcmp("do_softirq",funcname) ||
		!strcmp("tasklet_action",funcname) ||
		!strcmp("tasklet_hi_action",funcname) ||
		!strcmp("run_timer_list",funcname) ){
		return "BOTTOMHALF";
	}else if(!strncmp("sock_",funcname,strlen("sock_"))){
		return "SOCKET";
	}else if(!strncmp("tcp_",funcname,strlen("tcp_"))){
		return "TCP";
	}else if(!strcmp("do_general_protection",funcname) ||
		!strcmp("do_page_fault",funcname) ||
		!strcmp("do_debug",funcname) ){
		return "EXCEPTION";
	}else 
		return "KTAU_DEFAULT";
}

/* Function	: get_pid_info
 * Description	: locate and return the corresponded pid_info struct 
 * Input	
 * 	- pid_info_list
 *	- pid 
 * Output
 *	- THe pointer to the pid_info struct
 */
pid_info* get_pid_info(pid_info *pid_info_list[], int pid)
{
	int i = 0;

	for(i = 0; i< MAX_NUM_PID && pid_info_list[i] != NULL; i++){
		if(pid_info_list[i]->pid == pid){
			return(pid_info_list[i]);
		}
	}
	
	/* In case pid not found, initialize a new one */
	if(i >= MAX_NUM_PID){
		printf("ERROR: Too many pids.\n");
		return(NULL);
	}
	pid_info_list[i] = (pid_info*)malloc(sizeof(pid_info));
	pid_info_list[i]->pid	 	 = pid;
	pid_info_list[i]->num_templ	 = 0;
	pid_info_list[i]->num_aggregates = 0;
	pid_info_list[i]->num_user_ev	 = 0;
	memset(pid_info_list[i]->prev_e_buf,0,(sizeof(prof_e)*PROF_SIZE));
	memset(pid_info_list[i]->prof_e_buf,0,(sizeof(prof_e)*PROF_SIZE));
	memset(pid_info_list[i]->tmp_e_buf,0,(sizeof(prof_e)*PROF_SIZE));
	return(pid_info_list[i]);
}

int DumpKtauProfile(pid_info* info,char *output_path,double tsc){
	char output_dir[MAX_CHAR_LEN];	
	char output_file[MAX_CHAR_LEN];	
	int j;
	prof_e* ptr = NULL;

	sprintf(output_file,"%s/profile.%u.0.0",output_path,info->pid);
	
	/* Print number of templated_functions */
	ofstream fs_output (output_file , ios::out);
	if(!fs_output.is_open()){
		cout << "Error opening file: " << output_file << "\n";
		return(-1);
	}
		
	/* Print Header */
	fs_output << info->num_templ<< " templated_functions" << endl;
       	fs_output << PROF_HDR << endl;

	/* Print functions profile */
       for(j=0;j < PROF_SIZE; j++){
		ptr = &(info->tmp_e_buf[j]);
		//if(ptr->funcCount > 0){
		if(strlen(ptr->funcName) != 0){
			fs_output << "\"" 
				  << ptr->funcName << "()\" "		//Name
				  << ptr->funcCount << " "         	//Calls
				  << ptr->funcSubrs << " "             	//Subrs
				  << ptr->funcExcl/(tsc*1e-6) << " "   		//Excl in microsec
				  << ptr->funcIncl/(tsc*1e-6) << " "         	//Incl in microsec
				  << 0 << " "      			//ProfileCalls
				  << "GROUP=\"" << ptr->funcGrp << "\""  //Group
				  << endl;
		}
	}
	
	/* Print Aggregates */
	fs_output << info->num_aggregates << " aggregates" << endl;
	fs_output << info->num_user_ev << " userevents" << endl;
	fs_output << PROF_EV_HDR << endl;
	fs_output.close();	
	return(0);
}


/* Function	: mapping_profile
 * Description	: Perform function address to name mapping
 * Input	
 *	- input_path	= path to trace input
 *	- kallsym_map 	= mapping table
 *	- kallsyms_size = size of mapping table
 * 	- pid_info_list	= list of pid_info
 * 	- output_path	= path to output
 * Output
 *	- return 0 when complete
 */
int mapping_profile(char* input_path,
	addr_map* kallsyms_map,	
	int kallsyms_size,
	pid_info *pid_info_list[], 
	char *output_path,
	double *tsc)
{
	int no_PIDs = 0;
        char line[LINE_SIZE];
	char funcname[MAX_CHAR_LEN];
	char output_path_tmp[MAX_CHAR_LEN];
        char *ptr = line;
        char *addr= NULL;
        char *tmp = NULL;
        int  i;
        long long  par;
        unsigned int funcID;
        unsigned int addr_hex;
        unsigned long long  timestamp_usec;
        double timestamp;
	int pid;
	pid_info *cur_pid_info = NULL;
	prof_e *cur_prof_e = NULL;
	prof_e *prev_prof_e = NULL;
	prof_e *tmp_prof_e = NULL;

        /* Open inputfile and read */
	if(DEBUG)printf("mapping_profile : input_path = %s\n",input_path);
        ifstream fs_input (input_path,ios::in);
        if(!fs_input.is_open()){
                cout << "Error opening file: " << input_path << "\n";
                return(-1);
        }

	/*********** START PARSING INPUT FILE ********/
	/* The First line tell TSC */
	fs_input.getline(line,LINE_SIZE);
	ptr = line;
	if(!strncmp("TSC",ptr,strlen("TSC"))){
		addr = strtok_r(ptr,":",&tmp);
		*tsc = atof(tmp);
		if(DEBUG)printf("TSC            = %f\n",tsc);	
	}else{
		printf("Cannot determine TSC rate.\n");
		exit(1);
	}

	/* The second line tell how many PIDs */
	fs_input.getline(line,LINE_SIZE);
	ptr = line;	
	if(!strncmp("No Profiles",ptr,strlen("No Profiles"))){
		addr = strtok_r(ptr,":",&tmp);
		no_PIDs = atoi(tmp); 
		if(DEBUG)printf("Number of profile = %d\n",no_PIDs);	
	}else{
		printf("Cannot determine Number of PIDs.\n");
		exit(1);
	}

        for(fs_input.getline(line,LINE_SIZE) ; !fs_input.eof();
		fs_input.getline(line,LINE_SIZE))
	{
                ptr = line;

		/* Junk */
		if(!strncmp("No Profiles",ptr,strlen("No Profiles"))){
			continue;	
		}

		/* We have to use pid to search for the 
		 * corresponded pid_info and update 
		 * cur_pid_info
		 */	
		if(!strncmp("PID",ptr,strlen("PID")))
		{
			addr = strtok_r(ptr," ",&tmp);  	/* "PID: "*/
			addr = strtok_r(tmp," ",&tmp);  	/* Actual pid */	
			pid = atoi(addr);
		
			cur_pid_info = get_pid_info(pid_info_list, pid);
			continue;
		}
		//****************************************************************************
		/* Now processing each line of profile */
		if(!strncmp("Entry",ptr,strlen("Entry")))
		{
			addr = strtok_r(ptr," ",&tmp);			/* Entry */
			addr = strtok_r(tmp," ",&tmp);			/* <index>: */
			sscanf(addr,"%u:",&funcID);

			cur_prof_e = &(cur_pid_info->prof_e_buf[funcID]);
			prev_prof_e = &(cur_pid_info->prev_e_buf[funcID]);
			tmp_prof_e = &(cur_pid_info->tmp_e_buf[funcID]);
			
			if(strlen(prev_prof_e->funcName) == 0){
				cur_pid_info->num_templ++;	
			}

			/*ADDR*/
			addr = strtok_r(tmp," ",&tmp);			/* addr */
			addr = strtok_r(tmp," ",&tmp);			/* <address>,*/
			sscanf(addr,"%x,",&addr_hex);

			/*COUNT*/	
			addr = strtok_r(tmp," ",&tmp);			/* count */
			addr = strtok_r(tmp," ",&tmp);			/* <count> */
			sscanf(addr,"%u,",&(cur_prof_e->funcCount));
			
			/*INCL*/	
			addr = strtok_r(tmp," ",&tmp);			/* incl */
			addr = strtok_r(tmp," ",&tmp);			/* <incl>*/
			sscanf(addr,"%llu,",&(cur_prof_e->funcIncl));

			/*EXCL*/
			addr = strtok_r(tmp," ",&tmp);			/* excl */
			addr = strtok_r(tmp," ",&tmp);			/* <excl>*/
			sscanf(addr,"%llu",&(cur_prof_e->funcExcl));

			cur_prof_e->funcSubrs = 0;

			/* Searching through the mapping table to map addr to NAME */
			for(i=0;i<kallsyms_size;i++){
				if(addr_hex == kallsyms_map[i].addr){
					sprintf(cur_prof_e->funcName,"%s", kallsyms_map[i].name);
					break;
				}
			}
			/* GROUP */
			sprintf(cur_prof_e->funcGrp,"%s",get_ktau_group(cur_prof_e->funcName));
		
			if(cur_prof_e->funcExcl > cur_prof_e->funcIncl){
				printf("ERROR: Excl is larger than Incl:\n\
					PID		: %u\n\
					funcName	: %s\n\
					Excl		: %llu\n\
					Incl		: %llu\n",
					cur_pid_info->pid,
					cur_prof_e->funcName,
					cur_prof_e->funcExcl,
					cur_prof_e->funcIncl);
			}	
			
			/*Check prev and diff */
			if(strlen(prev_prof_e->funcName) == 0){
				memcpy(prev_prof_e, cur_prof_e, sizeof(prof_e));
			
				strcpy(tmp_prof_e->funcName, cur_prof_e->funcName);
				strcpy(tmp_prof_e->funcGrp, cur_prof_e->funcGrp);
				tmp_prof_e->funcSubrs 	= 0;
				tmp_prof_e->funcCount 	= 0;
				tmp_prof_e->funcIncl 	= 0;
				tmp_prof_e->funcExcl 	= 0;
		}else{
				/* Diff */
				strcpy(tmp_prof_e->funcName, cur_prof_e->funcName);
				strcpy(tmp_prof_e->funcGrp, cur_prof_e->funcGrp);
				tmp_prof_e->funcSubrs 	= 0;
				tmp_prof_e->funcCount 	= cur_prof_e->funcCount - prev_prof_e->funcCount;
				tmp_prof_e->funcIncl 	= cur_prof_e->funcIncl - prev_prof_e->funcIncl;
				tmp_prof_e->funcExcl 	= cur_prof_e->funcExcl - prev_prof_e->funcExcl;
				
			}
		}
		//****************************************************************************
	}

        /* Close input file */
        fs_input.close();
	return(0);
}
	
int main (int argc, char** argv)
{
	char output_path[MAX_CHAR_LEN];
	char output_dir[MAX_CHAR_LEN];
	char input_path[MAX_CHAR_LEN];
	char map_path[MAX_CHAR_LEN];
	int is_mult_trace = 0;
	int kallsyms_size = 0;
	int c,i;
	double tsc;
	addr_map kallsyms_map[MAP_SIZE];
	//t_ev *ev_buf, *cur;
	pid_info *pid_info_list[MAX_NUM_PID]={NULL};


        /************* Parsing Options **************/
        if(argc == 1){
                usage(argv[0]);
                exit(-1);
        }

        /* 
         * If using -f <config file> , then other command line option must not 
         * be used and vice versa. 
         */
	/* Command line options */
	while((c = getopt(argc,argv,"i:o:m:h")) != EOF){
		switch(c){
		case 'h': /* help */
			usage(argv[0]);
			exit(-1);
			break;
		case 'i': /* Input */
			strcpy(input_path,optarg);			
			break;
		case 'o': /* Output */
			strcpy(output_path,optarg);			
			break;
		case 'm': /* Map */
			strcpy(map_path,optarg);			
			break;
		case 'M': /* Multiple trace */
			is_mult_trace = 1;
			break;
		default:
			printf("Invalid arguement.\n");
			usage(argv[0]);
			exit(-1);
		}
	}
	
	/* Read Mapping */
	if(DEBUG)printf("... Reading Mapping file.\n");
	kallsyms_size = ReadKallsyms(map_path,kallsyms_map);

	/* Mapping Profile Input */
	if(DEBUG)printf("...Mapping Profile file.\n");
	mapping_profile(input_path, kallsyms_map, kallsyms_size,pid_info_list, output_path, &tsc);	

        /* 
         * Create output directory ./Kprofile 
         */
        sprintf(output_dir,"%s",output_path);
        if(mkdir(output_dir,0777) == -1){
                perror("mkdir");
                exit(1);
        }

	for(i=0 ; i<MAX_NUM_PID && pid_info_list[i] != NULL ; i++)
	{
		/* Dumping Profile */
		if(DEBUG)printf("... Dumping Profile for pid = %d.\n",pid_info_list[i]->pid);
		DumpKtauProfile(pid_info_list[i], output_dir,tsc);	
	}
	exit(0);
}
