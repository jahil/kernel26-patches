
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
//#include <sys/types.h>

#include <linux/unistd.h>

using namespace std;

#define LINE_SIZE               1024    
#define OUTPUT_NAME_SIZE        100

#define DEBUG                   0
#define maxof(x,y)              ((x>=y)?x:y)

#include <ktau_proc_interface.h>
#include <ktau_proc_map.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * Function             : print_tau_profiles
 */
int print_tau_profiles(ktau_output* inprofiles, int no_profiles, int nodeid, ktau_sym_map* psymmap, unsigned long long timer_res_sec)
{
        int i=0,j=0;
        char output_path[OUTPUT_NAME_SIZE];
        char output_dir[OUTPUT_NAME_SIZE];
        o_ent *ptr;
        unsigned int cur_index = 0;
        unsigned int user_ev_counter = 0;
        unsigned int templ_fun_counter = 0;
	
	if(no_profiles <= 0 ){
		return -1;
	}
		
        /* 
         * Create output directory ./Kprofile 
         */
	if(nodeid >= 0) {
		sprintf(output_path,"./Kprofile.%d", nodeid);
		sprintf(output_dir,"./Kprofile.%d", nodeid);
	} else if(nodeid == -1) {
		sprintf(output_path,"./Kprofile.aggregate");
		sprintf(output_dir,"./Kprofile.aggregate");
	} else {
		fprintf(stderr, "Bad nodeid(%d) provided to print_tau_profiles. Failing. \n", nodeid);
		return -1;
	}
        if(mkdir(output_path,777) == -1){
                perror("print_tau_profiles: mkdir");
                return(-1);
        }
        if(chmod(output_path, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH ) != 0) {
                perror("print_tau_profiles: chmod");
                return(-1);
        }

        /*
         * Dumping output to the output file "./Kprofile.<rank>/profile.<pid>.0.0"
         */
        // Data format :
        // %d templated_functions
        // "%s %s" %ld %G %G  
        //  funcname type numcalls Excl Incl
        // %d aggregates
        // <aggregate info>

        /* For Each Process */
        for(i=0;i<no_profiles;i++){
                user_ev_counter = 0;
                templ_fun_counter = 0;

                /* Counting Profile */
                for(j=0;j < (inprofiles+i)->size; j++){
                        ptr = (((inprofiles)+i)->ent_lst)+j;
			if(ptr->entry.addr == 0) {
				continue;
			}
                        if(ptr->index < 300 || ptr->index > 399){
                                templ_fun_counter++;
                        }else{
                                user_ev_counter++;
                        }
                }
		
		if((inprofiles+i)->pid == -1) {
			sprintf(output_path,"%s/profile.0.0.0",output_dir, nodeid);
		} else {
			sprintf(output_path,"%s/profile.%u.0.0",output_dir, (inprofiles+i)->pid);
		}
                ofstream fs_output (output_path , ios::out);
                if(!fs_output.is_open()){
                        cerr << "print_tau_profiles: Error opening file: " << output_path << "\n";
                        return(-1);
                }

                /* OUTPUT: Templated function */
                fs_output << templ_fun_counter << " templated_functions" << endl;
                fs_output << "# Name Calls Subrs Excl Incl ProfileCalls" << endl;

                for(j=0;j < (inprofiles+i)->size; j++){
                        ptr = (((inprofiles)+i)->ent_lst)+j;
			if(ptr->entry.addr == 0) {
				continue;
			}
                        const char* func_name = ktau_lookup_sym(ptr->entry.addr, psymmap);
                        if(ptr->index < 300 || ptr->index > 399){
                                fs_output << "\"" << 
                                          func_name << "()\" "  //Name
                                          << ptr->entry.data.timer.count << " "         //Calls
                                          << 0 << " "                                   //Subrs
                                          << (double)ptr->entry.data.timer.excl/timer_res_sec //Excl
                                          << " "
                                          << (double)ptr->entry.data.timer.incl/timer_res_sec //Incl
                                          << " "
                                          << 0 << " ";                                  //ProfileCalls


                                if(!strcmp("schedule",func_name)){
                                        fs_output << "GROUP=\"TAU_KERNEL_MERGE | KTAU_SCHEDULE\"" << endl;
                                //}else if(!strcmp("__run_timers",func_name)){
                                //      fs_output << "GROUP=\"TAU_KERNEL_MERGE | KTAU_RUN_TIMERS\"" << endl;
                                }else if(strstr(func_name, "page_fault")){
                                        fs_output << "GROUP=\"TAU_KERNEL_MERGE | KTAU_EXCEPTION\"" << endl;
                                }else if(strstr(func_name, "IRQ")){
                                        fs_output << "GROUP=\"TAU_KERNEL_MERGE | KTAU_IRQ\"" << endl;
                                }else if(strstr(func_name, "run_timers")){
                                        fs_output << "GROUP=\"TAU_KERNEL_MERGE | KTAU_BH\"" << endl;
                                }else if(strstr(func_name, "workqueue")){
                                        fs_output << "GROUP=\"TAU_KERNEL_MERGE | KTAU_BH\"" << endl;
                                }else if(strstr(func_name, "tasklet")){
                                        fs_output << "GROUP=\"TAU_KERNEL_MERGE | KTAU_BH\"" << endl;
                                //}else if(!strcmp("__do_softirq",func_name)){
                                //      fs_output << "GROUP=\"TAU_KERNEL_MERGE | KTAU_DO_SOFTIRQ\"" << endl;
                                }else if(strstr(func_name, "softirq")){
                                        fs_output << "GROUP=\"TAU_KERNEL_MERGE | KTAU_BH\"" << endl;
                                }else if(strstr(func_name, "sys_")){
                                        if(strstr(func_name, "sock")){
                                                fs_output << "GROUP=\"TAU_KERNEL_MERGE | KTAU_SYSCALL | KTAU_SOCK\"" << endl;
                                        } else if(strstr(func_name, "read")){
                                                fs_output << "GROUP=\"TAU_KERNEL_MERGE | KTAU_SYSCALL | KTAU_IO\"" << endl;
                                        } else if(strstr(func_name, "write")){
                                                fs_output << "GROUP=\"TAU_KERNEL_MERGE | KTAU_SYSCALL | KTAU_IO\"" << endl;
                                        } else if(strstr(func_name, "send")){
                                                fs_output << "GROUP=\"TAU_KERNEL_MERGE | KTAU_SYSCALL | KTAU_IO\"" << endl;
                                        } else if(strstr(func_name, "recv")){
                                                fs_output << "GROUP=\"TAU_KERNEL_MERGE | KTAU_SYSCALL | KTAU_IO\"" << endl;
                                        } else {
                                                fs_output << "GROUP=\"TAU_KERNEL_MERGE | KTAU_SYSCALL\"" << endl;
                                        }
                                }else if(strstr(func_name, "tcp")){
                                        fs_output << "GROUP=\"TAU_KERNEL_MERGE | KTAU_TCP\"" << endl;
                                }else if(strstr(func_name, "icmp")){
                                        fs_output << "GROUP=\"TAU_KERNEL_MERGE | KTAU_ICMP\"" << endl;
                                }else if(strstr(func_name, "sock")){
                                        fs_output << "GROUP=\"TAU_KERNEL_MERGE | KTAU_SOCK\"" << endl;
                                }else{
                                        fs_output << "GROUP=\"TAU_KERNEL_MERGE | KTAU_DEFAULT\"" << endl;
                                }



				/*
                                if(!strcmp("schedule",func_name)){
                                        fs_output << "GROUP=\"KTAU_SCHEDULE\"" << endl;
                                }else if(!strcmp("__run_timers",func_name)){
                                        fs_output << "GROUP=\"KTAU_RUN_TIMERS\"" << endl;
                                }else if(strstr(func_name, "softirq"))  {
                                        fs_output << "GROUP=\"KTAU_SOFTIRQ\"" << endl;
                                }else if(strstr(func_name, "sys_")){
                                        fs_output << "GROUP=\"KTAU_SYSCALL\"" << endl;
                                }else if(strstr(func_name, "sock_")){
                                        fs_output << "GROUP=\"KTAU_SOCKET\"" << endl;
                                }else if(strstr(func_name, "tcp_")){
                                        fs_output << "GROUP=\"KTAU_TCP\"" << endl;
                                }else if(strstr(func_name, "icmp_")){
                                        fs_output << "GROUP=\"KTAU_ICMP\"" << endl;
                                }else{
                                        fs_output << "GROUP=\"KTAU_DEFAULT\"" << endl;
                                }
				*/
                        }
                }
                /* OUTPUT: Aggregates*/ 
                fs_output << 0 << " aggregates" << endl;
                
                /* OUTPUT: User-events*/
                fs_output << user_ev_counter  << " userevents" << endl;
                fs_output << "# eventname numevents max min mean sumsqr"<< endl;

                for(j=0;j < (inprofiles+i)->size; j++){
                        ptr = (((inprofiles)+i)->ent_lst)+j;
			if(ptr->entry.addr == 0) {
				continue;
			}
                        const char* ev_name = ktau_lookup_sym(ptr->entry.addr, psymmap);
                        if(ptr->index >= 300 && ptr->index <= 399 ){
                                if(ptr->entry.data.timer.count != 0){
                                        fs_output << "\"Event_"
                                          << ev_name << "()\" "         //eventname
                                          << ptr->entry.data.timer.count << " "         //numevents
                                          << 1 << " "                                   //max
                                          << 1 << " "                                   //min
                                          << 1 << " "                                   //mean
                                          << 1 << " "                                   //sumsqr
                                          << endl;
                                }else{
                                        fs_output << "\"Event_"
                                          << ev_name << "()\" "         //eventname
                                          << ptr->entry.data.timer.count << " "         //numevents
                                          << 0 << " "                                   //max
                                          << 0 << " "                                   //min
                                          << 0 << " "                                   //mean
                                          << 0 << " "                                   //sumsqr
                                          << endl;

                                }
                        }
                }
                        
                fs_output.close();
        }       
        return 0;
}               

#ifdef __cplusplus
}
#endif
