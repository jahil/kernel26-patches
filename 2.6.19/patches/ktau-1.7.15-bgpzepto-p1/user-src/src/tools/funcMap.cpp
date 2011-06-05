/*************************************************************
 * File         : user-src/src/funcMap.cpp
 * Version      : $Id: funcMap.cpp,v 1.4 2006/11/12 00:26:31 anataraj Exp $
 *
 ************************************************************/

#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define NAME_SIZE       50
//#define MAP_SIZE        10 * 1024
#define MAP_SIZE        1024 * 1024
#define LINE_SIZE	500
#define DEBUG		0
using namespace std;

#define STR2LL(x,y,z) ((unsigned int)strtoul(x,y,z))
//#define STR2LL(x,y,z) (strtoll(x,y,z))

typedef struct _addr_map{
        unsigned long long addr;
        char name[NAME_SIZE];
}addr_map;


addr_map kallsyms_map[MAP_SIZE];
int kallsyms_size = 0;
int ReadKallsyms(char *kallsyms_path)
{
        char line[LINE_SIZE];
        char *ptr = line;
        char *addr;
        char *flag;
        char *func_name;
        unsigned int stext = 0;
        int found_stext = 0;

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
		if(!addr) {
			continue;
		}
                flag = strtok(NULL," ");
                func_name = strtok(NULL," ");

                if(!strcmp("T",flag) || !strcmp("t",flag)){
                        if(!found_stext && !strcmp("stext",func_name)){
                                kallsyms_map[kallsyms_size].addr = STR2LL(addr,NULL,16);
                                strcpy(kallsyms_map[kallsyms_size++].name, func_name);
                                found_stext = 1;
                                if(DEBUG)printf("ReadKallsyms: address %llx , name %s \n",
                                                kallsyms_map[kallsyms_size-1].addr,
                                                kallsyms_map[kallsyms_size-1].name);
                        } else{
                                kallsyms_map[kallsyms_size].addr = STR2LL(addr,NULL,16);
                                strcpy(kallsyms_map[kallsyms_size++].name, func_name);
                                if(DEBUG)printf("ReadKallsyms: address %llx , name %s \n",
                                                kallsyms_map[kallsyms_size-1].addr,
                                                kallsyms_map[kallsyms_size-1].name);
                        }
                } /* else if(!strcmp("_etext",func_name)){
                        kallsyms_map[kallsyms_size].addr = STR2LL(addr,NULL,16);
                        strcpy(kallsyms_map[kallsyms_size++].name, func_name);
                        if(DEBUG)printf("ReadKallsyms: address %llx , name %s \n",
                                                kallsyms_map[kallsyms_size-1].addr,
                                                kallsyms_map[kallsyms_size-1].name);
                        break;
                } */
        }
        /* Close /proc/kallsyms */
        fs_kallsyms.close();

        return kallsyms_size;
}

int mapping_trace(char* input_path){
	char line[LINE_SIZE];
	char *ptr = line;
	char *tok= NULL;
	char *tmp = NULL;
	int  i;
	unsigned long long  addr_hex;

        /* Open and read file */
        ifstream fs_input (input_path,ios::in);
        if(!fs_input.is_open()){
                cout << "Error opening file: " << input_path << "\n";
                return(-1);
        }
	
	/* Now processing each line */		
	while(!fs_input.eof()){
		fs_input.getline(line,LINE_SIZE);
		ptr = line;
		tok = strtok_r(ptr," ",&tmp);//tok HAS TIME
		if(tok == NULL) exit(0);
		//if(tmp == NULL) exit(0);

		if((strncmp("PID:",tok,strlen("PID")) == 0 ) || 
		   (strncmp("TSC" ,tok,strlen("TSC")) == 0)  || 
		   (strncmp("No" ,tok,strlen("No")) == 0))
		{
			/* Just print this line */
			cout << tok << " " << tmp << "\n";
		}else{
			cout << tok << " ";
			
			tok = strtok_r(NULL," ",&tmp); //tok HAS ADDR
			if(tok == NULL) exit(0);
			if(tmp == NULL) exit(0);

			//tmp SHOULD HAVE 0 or 1
			if( atoi(tmp) == 0)
				cout << " in  ";
			else
				cout << " out ";

			addr_hex = STR2LL(tok,NULL,16);

			if(addr_hex == 0) {
				//ktau_trace_loss
				cout << " " << "ktau_trace_loss" << "\n"; 
			} else {
				/* Searching through the mapping table */	
				for(i=0;i<kallsyms_size;i++){
					if(addr_hex == kallsyms_map[i].addr){
						cout << " " << kallsyms_map[i].name << "\n"; 
						break;	
					}			
				}
			}

		}
	}
	
        /* Close /proc/kallsyms */
        fs_input.close();
}

int mapping_profile(char* input_path){
	char line[LINE_SIZE];
	char *ptr = line;
	char *addr= NULL;
	char *tmp = NULL;
	char *tmp1 = NULL;
	int  i;
	unsigned long long  addr_hex;

        /* Open and read file */
        ifstream fs_input (input_path,ios::in);
        if(!fs_input.is_open()){
                cout << "Error opening file: " << input_path << "\n";
                return(-1);
        }
	
	/* Now processing each line */		
	while(!fs_input.eof()){
		fs_input.getline(line,LINE_SIZE);
		ptr = line;
		addr = strtok_r(ptr," ",&tmp);
		if(addr == NULL) exit(0);
		if(tmp == NULL) exit(0);

		if(!strcmp("Entry",addr)){
			cout << addr << " ";
			addr = strtok_r(tmp," ",&tmp);
			cout << addr << " ";
			addr = strtok_r(tmp," ",&tmp);
			cout << addr << " ";
			addr = strtok_r(tmp," ",&tmp);
			addr = strtok_r(addr,",",&tmp1);
			if(addr == NULL) exit(0);
			if(tmp == NULL) exit(0);
			addr_hex = STR2LL(addr,NULL,16);

			/* Searching through the mapping table */	
			for(i=0;i<kallsyms_size;i++){
				if(addr_hex == kallsyms_map[i].addr){
					printf("%30s %s\n",kallsyms_map[i].name,tmp);	
					break;	
				}			
			}

		}else{
			/* Just print this line */
			cout << addr << " " << tmp << "\n";
		}
	}
	
        /* Close /proc/kallsyms */
        fs_input.close();
}

void usage(void){
	printf("USAGE: funcMap <-h|-t|-p> <Mapping File> <Input File>\n");
}

int main(int argc, char* argv[]){
	int map_trace = 1;	

	if(argc < 2 || argc > 4){
		printf(" Invalid arguements:\n");
		usage();
		exit(0);
	}
	
	if(!strcmp(argv[1], "-h")){
		usage();
		exit(0);
	}

	if(!strcmp(argv[1], "-p")){
		map_trace = 0;	
	}else if(!strcmp(argv[1], "-t")){
		map_trace = 1;	
	}else{
		printf(" Invalid arguements:\n");
		usage();
		exit(0);
	}

	/* Read Map */
	printf("Reading Map\n");
	ReadKallsyms(argv[2]);
	if(map_trace){
		printf("Mapping trace\n");
		mapping_trace(argv[3]);
	}else{
		printf("Mapping profile\n");
		mapping_profile(argv[3]);
	}
	exit(0);	
		
}
