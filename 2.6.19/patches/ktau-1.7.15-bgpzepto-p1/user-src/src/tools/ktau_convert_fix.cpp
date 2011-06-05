/*************************************************************
 * File         : user-src/src/tools/ktau_convert_fix.cpp
 * Version      : $Id: ktau_convert_fix.cpp,v 1.5 2006/11/12 00:26:31 anataraj Exp $
 *
 ************************************************************/

#include <iostream>
#include <fstream>
#include <stack>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

using namespace std;

#define MAX_FUNC_NAME 	100
#define MAX_NUM_EV	1024
#define DEBUG		1
extern "C"
{
#include "TAU_trace.h"
}

typedef struct
{
	t_ev	ev;
	char	funcName[MAX_FUNC_NAME];
} tau_ev_entry; 

/* Function     : usage
 * Description  : Print out usage for this program
 * Input        
 *      - name = program name
 */
void usage(char* name)
{
        printf("usage: %s <-h> <-i input> <-o output > <-m map> \n",name);
}


int main(int argc, char* argv[]){
        char input_path[MAX_CHAR_LEN];
	char output_path[MAX_CHAR_LEN];
	t_ev no_entry_ev[MAX_NUM_EV];
	t_ev no_exit_ev[MAX_NUM_EV];
	t_ev ktau_lost_ev[MAX_NUM_EV];
	t_ev swap_buffer_ev[MAX_NUM_EV];
	int num_no_entry_ev = 0;
	int num_no_exit_ev = 0;
	int num_ktau_lost_ev = 0;
	int num_swap_ev = 0;
        int c,i;
	FILE *input_fd, *output_fd;
	x_uint64 first_ti;
	stack<t_ev>  ev_stack;
	t_ev *ptr = (t_ev*)malloc(sizeof(t_ev));
	t_ev *ptr_top;	
	int cur_kle = 0, cur_nee = 0;	
        /************* Parsing Options **************/
        if(argc == 1){
                usage(argv[0]);
                exit(-1);
        }

        /* Command line options */
        while((c = getopt(argc,argv,"i:o:h")) != EOF){
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
                default:
                        printf("Invalid arguement.\n");
                        usage(argv[0]);
                        exit(-1);
                }
        }


	/******************** PASS 1 **********************/	
	/* Read the trace file */
        if((input_fd = fopen(input_path,"r")) == NULL ){
                perror("fopen");
                exit(-1);
        }
	
	while(fread(ptr,1,sizeof(t_ev),input_fd)){

		/* For each event, 
		 * - Entry: store in a stack
		 * - Exit : pop the stack
		 */
		if(ptr->par == FUNC_ENTER){
			ev_stack.push(*ptr);
		}else if(ptr->par == FUNC_EXIT){
			if(!ev_stack.empty()){
				ptr_top = &(ev_stack.top());
			
				if(ptr_top->ev == ptr->ev){
					ev_stack.pop();
				}else{
					if(DEBUG){
						printf("-----> Not matching trace event for :");
						printf("ev : %d, ", ptr->ev);
						printf("nid : %u, ", ptr->nid);
						printf("tid : %u, ", ptr->tid);
						printf("par : %2lld, ", ptr->par);
						printf("ti : %lld\n", ptr->ti);
					}
					memcpy(&(no_entry_ev[num_no_entry_ev]),ptr,sizeof(t_ev)); 
					num_no_entry_ev++;
				}
			}else{
				if(DEBUG){
					printf("-----> Stack is empty..............:");
					printf("ev : %d, ", ptr->ev);
					printf("nid : %u, ", ptr->nid);
					printf("tid : %u, ", ptr->tid);
					printf("par : %2lld, ", ptr->par);
					printf("ti : %lld\n", ptr->ti);
				}
				memcpy(&(no_entry_ev[num_no_entry_ev]),ptr,sizeof(t_ev)); 
				num_no_entry_ev++;
			}
		
			/* Check if this is a lost event */	
			if(ptr->ev == 1){
				if(DEBUG){
					printf("-----> Found ktau_lost_event       :");
					printf("ev : %d, ", ptr->ev);
					printf("nid : %u, ", ptr->nid);
					printf("tid : %u, ", ptr->tid);
					printf("par : %2lld, ", ptr->par);
					printf("ti : %lld\n", ptr->ti);
				}
				memcpy(&(ktau_lost_ev[num_ktau_lost_ev]),ptr,sizeof(t_ev)); 
				num_ktau_lost_ev++;
			}
		}
	}
	/* Now we have to dump the stack to get the no_exit_ev */
	printf("No exit events :\n");
	while(!ev_stack.empty()){
		ptr_top = &(ev_stack.top());
		memcpy(&(no_exit_ev[num_no_exit_ev]),&(ev_stack.top()),sizeof(t_ev)); 
		num_no_exit_ev++;
		ev_stack.pop();
		if(DEBUG){
			printf("ev : %d, ", ptr_top->ev);
			printf("nid : %u, ", ptr_top->nid);
			printf("tid : %u, ", ptr_top->tid);
			printf("par : %2lld, ", ptr_top->par);
			printf("ti : %lld\n", ptr_top->ti);
		}
	}	
	fclose(input_fd);	

	/**************************** PASS 2 **************************************/
	/* Now, we have to put the matching Entry event
	 * for the exit event that doesn't have one.
	 */
	/* Open the trace file */
        if((input_fd = fopen(input_path,"r")) == NULL ){
                perror("fopen");
                exit(-1);
        }
	
	/* Open the output file */
	sprintf(output_path,"%s.fix",input_path);
        if((output_fd = fopen(output_path,"w+")) == NULL ){
                perror("fopen");
                exit(-1);
        }

	/* Copying file over */	
	cur_kle = 0;
	cur_nee  = 0;
	num_swap_ev = 0;	

	while(fread(ptr,1,sizeof(t_ev),input_fd)){
		if(ptr->ev == 1 && 
			ptr->par== FUNC_ENTER &&
			ptr->ti < ktau_lost_ev[cur_kle].ti){  /* This is the entry of ktau_lost_event */

				while(num_no_exit_ev > 0 && ptr->ti > no_exit_ev[num_no_exit_ev-1].ti){
					no_exit_ev[num_no_exit_ev-1].par = FUNC_EXIT;
					no_exit_ev[num_no_exit_ev-1].ti = ptr->ti;
					memcpy(&(swap_buffer_ev[num_swap_ev]),&(no_exit_ev[num_no_exit_ev-1]),sizeof(t_ev)); 
					num_no_exit_ev--;
					num_swap_ev++;
				}		
				
			/* Dump swap_buffer_ev */
			for(i=num_swap_ev-1; i>=0; i--){
				fwrite(&(swap_buffer_ev[i]),1,sizeof(t_ev),output_fd);
			}
			num_swap_ev = 0;
		
			fwrite(ptr,1,sizeof(t_ev),output_fd); /* Write out the entry of ktau_lost_event */
		}else if(ptr->ev == 1 && 
			ptr->par== FUNC_EXIT && 
			ptr->ti == ktau_lost_ev[cur_kle].ti){ /* This is the exit of ktau_lost_event */
			fwrite(ptr,1,sizeof(t_ev),output_fd);
			cur_kle++;
		
			/* Then write out the entry events */
			while( cur_kle < num_ktau_lost_ev && cur_nee < num_no_entry_ev){
				if(ktau_lost_ev[cur_kle].ti > no_entry_ev[cur_nee].ti) {
					no_entry_ev[cur_nee].par = FUNC_ENTER;
					no_entry_ev[cur_nee].ti = ptr->ti;
					memcpy(&(swap_buffer_ev[num_swap_ev]),&(no_entry_ev[cur_nee]),sizeof(t_ev)); 
					num_swap_ev++;
					cur_nee++;
				}else{
					break;
				}
			}
			/* Case of the last ktau_lost_event */
			if(cur_kle == num_ktau_lost_ev && cur_nee < num_no_entry_ev){
				while(cur_nee < num_no_entry_ev){
					no_entry_ev[cur_nee].par = FUNC_ENTER;
					no_entry_ev[cur_nee].ti = ptr->ti;
					memcpy(&(swap_buffer_ev[num_swap_ev]),&(no_entry_ev[cur_nee]),sizeof(t_ev)); 
					num_swap_ev++;
					cur_nee++;
				}
			}	
		
			/* Dump swap_buffer_ev */
			for(i=num_swap_ev-1; i>=0; i--){
				fwrite(&(swap_buffer_ev[i]),1,sizeof(t_ev),output_fd);
			}
			num_swap_ev = 0;	
				
		}else{
			fwrite(ptr,1,sizeof(t_ev),output_fd);
		}
	}

	fclose(input_fd);
	fclose(output_fd);

	exit(0);
}
