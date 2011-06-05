/*************************************************************
 * File         : user-src/src/ktau_proc_map.cpp
 * Version      : $Id: ktau_proc_map.cpp,v 1.1 2006/11/12 00:26:30 anataraj Exp $
 ************************************************************/

#include <iostream>
#include <fstream>
#include <string>
#include <map>

#define NAME_SIZE       50
#define MAP_SIZE        1024 * 1024
#define LINE_SIZE	1024
#define DEBUG		0
using namespace std;

#include <ktau_proc_map.h>

#define RUNKTAU_STR2LL strtoul

//stl map to hold the addr-to-name lookup data
typedef map<unsigned long long, string> KERN_SYMTAB;

struct ktau_sym_map {
	KERN_SYMTAB table;
};


const char* ktau_lookup_sym(unsigned long long addr, struct ktau_sym_map* pmap) {
	return (pmap->table)[addr].c_str();
}


void ktau_put_kallsyms(struct ktau_sym_map* pmap) {
	delete pmap;
}


ktau_sym_map* ktau_get_kallsyms(char* filepath)
{
        char line[LINE_SIZE];
        char *ptr = line;
        char *addr;
        char *flag;
        char *func_name;
        unsigned int stext = 0;
        int found_stext = 0;

	ktau_sym_map* pmap = new ktau_sym_map();
	if(!pmap) {
                cerr << "new failed: memory problem." << "\n";
		return NULL;
	}
	KERN_SYMTAB& table = pmap->table;

        /* Open and read file */
        ifstream fs_kallsyms (filepath,ios::in);
        if(!fs_kallsyms.is_open()){
                cout << "Error opening file: " << filepath << "\n";
                return(NULL);
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
                                table[(unsigned int)RUNKTAU_STR2LL(addr,NULL,16)] = string(func_name);
                                //kallsyms_map[kallsyms_size].addr = (unsigned int)RUNKTAU_STR2LL(addr,NULL,16);
                                //strcpy(kallsyms_map[kallsyms_size++].name, func_name);
                                found_stext = 1;
                                if(DEBUG)printf("ReadKallsyms: address %llx , name %s \n",
                                                (unsigned int)RUNKTAU_STR2LL(addr,NULL,16),
                                                table[(unsigned int)RUNKTAU_STR2LL(addr,NULL,16)].c_str());
                        } else{
                                table[(unsigned int)RUNKTAU_STR2LL(addr,NULL,16)] = string(func_name);
                                //kallsyms_map[kallsyms_size].addr = (unsigned int)RUNKTAU_STR2LL(addr,NULL,16);
                                //strcpy(kallsyms_map[kallsyms_size++].name, func_name);
                                if(DEBUG)printf("ReadKallsyms: address %llx , name %s \n",
                                                (unsigned int)RUNKTAU_STR2LL(addr,NULL,16),
                                                table[(unsigned int)RUNKTAU_STR2LL(addr,NULL,16)].c_str());
                        }
                }else if(!strcmp("_etext",func_name)){
                        table[(unsigned int)RUNKTAU_STR2LL(addr,NULL,16)] = string(func_name);
                        //kallsyms_map[kallsyms_size].addr = (unsigned int)RUNKTAU_STR2LL(addr,NULL,16);
                        //strcpy(kallsyms_map[kallsyms_size++].name, func_name);
                        if(DEBUG)printf("ReadKallsyms: address %llx , name %s \n",
                                        (unsigned int)RUNKTAU_STR2LL(addr,NULL,16),
                                        table[(unsigned int)RUNKTAU_STR2LL(addr,NULL,16)].c_str());
                        break;
                }
        }
        /* Close /proc/kallsyms */
        fs_kallsyms.close();

        return pmap;
}

