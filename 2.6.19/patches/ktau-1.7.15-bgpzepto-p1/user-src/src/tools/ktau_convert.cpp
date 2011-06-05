/*************************************************************
 * File         : user-src/src/tools/ktau_convert.cpp
 * Version      : $Id: ktau_convert.cpp,v 1.11 2006/11/12 00:26:31 anataraj Exp $
 *
 ************************************************************/

#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

extern "C"
{
#include "TAU_trace.h"
}

using namespace std;

#define MAP_SIZE		100*1024	
#define LINE_SIZE		500
#define NAME_SIZE		50
#define MAX_NUM_PID		100
#define DEBUG			0

typedef struct {
        unsigned long long addr;
        char name[NAME_SIZE];
}addr_map;

typedef struct {
	int 		pid;	
	t_ev 		*ev_buf; 
	t_ev 		*cur;
	x_uint64	last_ti;	
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
	pid_info_list[i]->pid	 = pid;
	pid_info_list[i]->ev_buf = NULL; 
	pid_info_list[i]->cur	 = NULL;
	return(pid_info_list[i]);
}


/* Function	: mapping_trace
 * Description	: Perform function address to name mapping
 * Input	
 *	- input_path	= path to trace input
 *	- kallsym_map 	= mapping table
 *	- kallsyms_size = size of mapping table
 * 	- ev_buf	= trace buffer
 * 	- cur		= current pointer of ev_buf
 * 	- id_buf	= function id buffer
 * Output
 *	- return 0 when complete
 */
int mapping_trace(char* input_path,
	addr_map* kallsyms_map,	
	int kallsyms_size,
	//t_ev **ev_buf, 
	//t_ev **cur,
	pid_info *pid_info_list[], 
	t_id **id_buf,
	char *output_path)
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
        unsigned long long  addr_hex;
        unsigned long long  timestamp_usec;
        double timestamp;
	double tsc;
	int pid;
	pid_info *cur_pid_info = NULL;

        /* Open inputfile and read */
	if(DEBUG)printf("mapping_trace : input_path = %s\n",input_path);
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
		tsc = atof(tmp);
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
		if(DEBUG)printf("Number of trace = %d\n",no_PIDs);	
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

		/* Now processing trace events */
                addr = strtok_r(ptr," ",&tmp);			/* Get timestamp */
		timestamp= atof(addr);
		timestamp_usec = (unsigned long long)((timestamp)/(tsc * 1e-6));
                        
		addr = strtok_r(tmp," ",&tmp); 			/* Get the function address*/
		addr_hex = strtoll(addr,NULL,16);

		/* Searching through the mapping table */
		if(addr_hex == 0){
			sprintf(funcname,"ktau_lost_event");

			/* If ktau has lost some events, there might be
			 * a chance that we might have lost just the entry event.
			 * Therefore, we have to account for it
			 */




		}else{
			for(i=0;i<kallsyms_size;i++){
				if(addr_hex == kallsyms_map[i].addr){
					sprintf(funcname,"%s", kallsyms_map[i].name);
					break;
				}
			}
		}
		/* Getting action */
		if( atoi(tmp) == 0){
			par = FUNC_ENTER;
		}else{
			par = FUNC_EXIT;
		}
		
		sprintf(output_path_tmp,"%s.%d.trc",output_path,pid);
		trace_write(&(cur_pid_info->ev_buf),
			    &(cur_pid_info->cur),
			    &(cur_pid_info->last_ti),
			    register_ev(id_buf,addr_hex,get_ktau_group(funcname),0,funcname,"EntryExit"),
			    pid,
			    0,
			    par,
			    timestamp_usec,
			    output_path_tmp);
	}

        /* Close input file */
        fs_input.close();
	return(0);
}
	
int main (int argc, char** argv){
	char output_path[MAX_CHAR_LEN];
	char output_path_tmp[MAX_CHAR_LEN];
	char input_path[MAX_CHAR_LEN];
	char map_path[MAX_CHAR_LEN];
	int is_mult_trace = 0;
	int kallsyms_size = 0;
	int c,i;
	addr_map kallsyms_map[MAP_SIZE];
	//t_ev *ev_buf, *cur;
	pid_info *pid_info_list[MAX_NUM_PID]={NULL};
	t_id *id_buf;


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

	/* Hack to put "ktau_lost_event to be the first one event in edf*/
        register_ev(&id_buf,0,get_ktau_group("ktau_lost_event"),0,"ktau_lost_event","EntryExit");
	
	/* Mapping Trace Input */
	if(DEBUG)printf("...Mapping trace file.\n");
	mapping_trace(input_path, kallsyms_map, kallsyms_size,pid_info_list, &id_buf,output_path);	
	//mapping_trace(input_path, kallsyms_map, kallsyms_size,&ev_buf, &cur, &id_buf);	

	for(i=0 ; i<MAX_NUM_PID && pid_info_list[i] != NULL ; i++)
	{
		/* We have to check whether the ev_buf actually have events */
		if((pid_info_list[i]->ev_buf) != NULL){
			/* Ending Trace */
			if(DEBUG)printf("... Ending trace for pid = %d.\n",pid_info_list[i]->pid);
			trace_end(&(pid_info_list[i]->ev_buf),
				  &(pid_info_list[i]->cur),
				  &(pid_info_list[i]->last_ti),
				  pid_info_list[i]->pid);
			/* Dumping Trace */
			if(DEBUG)printf("... Dumping trace file for pid = %d.\n",pid_info_list[i]->pid);
			sprintf(output_path_tmp,"%s.%d.trc",output_path,pid_info_list[i]->pid);	
			trace_dump(&(pid_info_list[i]->ev_buf),
				   &(pid_info_list[i]->cur),
				   output_path_tmp);	
		}
	}

	//trace_end(&ev_buf,&cur);
	//trace_dump(&ev_buf,&cur,output_path);	

	/* Dumping edf*/
	if(DEBUG)printf("... Dumping edf file.\n");
	edf_dump(&id_buf,output_path);	

	exit(0);
}
