/******************************************************************************
 * Version	: $Id: ktau_inject.c,v 1.1.2.1 2007/04/12 05:18:15 anataraj Exp $
 * ***************************************************************************/ 

//std kernel headers
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/smp.h>

//debug support
#define TAU_DEBUG
#define TAU_NAME "ktau_inject"
#include <linux/ktau/ktau_print.h>

#include <linux/ktau/ktau_inject.h>


//to hold values
volatile unsigned long long ktau_inject_val[KTAU_MAX_INJECT_VALS];

//to hold 'actual' flags
//One set of flags per-processor
volatile unsigned long long ktau_inject_flags[NR_CPUS][KTAU_MAX_INJECT_VALS];

//dummy result locations per-processor - just so that the buys loop is not optimized away!
volatile unsigned long long ktau_inject_dummy[NR_CPUS][KTAU_MAX_INJECT_VALS];

//a setter routine that runs on each processor, that copies new value into 'flag' for that processor
//just sets all the flags to the values (even if they havent changed)
void ktau_inject_setter(void* info) {
	int cpu = smp_processor_id();
	int i = 0;
	info("ktau_inject_setter: Enter: CPUID:%d PID:%d\n", cpu, current->pid);
	for(i=0; i<KTAU_MAX_INJECT_VALS; i++) {
		ktau_inject_flags[cpu][i] = ktau_inject_val[i];
	}
	info("ktau_inject_setter: Exit: CPUID:%d PID:%d\n", cpu, current->pid);
}


//a routine that performs the IPI and local-inject setting
int ktau_inject_IPI(void) {
	int ret = 0;

	//set yourself
	ktau_inject_setter(NULL);

	ret = smp_call_function(ktau_inject_setter, NULL, 0, 1/*wait until comp*/);
	if(ret) {
		err("ktau_inject_IPI: PID: smp_call_function returned ERROR[%d].\n", ret);
		return ret;
	}

	return 0;
}


//just sets 'values'
int ktau_inject_vals(unsigned long long* vals, int num) {
	int i = 0;
	info("ktau_inject_vals: Enter: CPUID:%d PID:%d\n", smp_processor_id(), current->pid);

	if(num < 0 || num > KTAU_MAX_INJECT_VALS) {
		err("ktau_inject_vals: bad num:%d.\n", num);
		return -1;
	}

	for(i=0; i<num; i++) {
		ktau_inject_val[i] = vals[i];
	} 

	info("ktau_inject_vals: Exit: CPUID:%d PID:%d\n", smp_processor_id(), current->pid);

	return 0;
}



#if 0
//a piece of inline code that busy-loops for 'flag' number of times
inline void ktau_inject_now(int flagno) {
	unsigned long long lc_flag = ktau_inject_flags[smp_processor_id()][flagno];
	volatile unsigned long long a = 59, b = 43;
	while(lc_flag--) {
		a += (ktau_inject_dummy[smp_processor_id()][flagno] + (b/a) - 32);
	}
	ktau_inject_dummy[smp_processor_id()][flagno] = b+a;
}
#endif /* 0 */

