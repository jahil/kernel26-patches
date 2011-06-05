/******************************************************************************
 * Version	: $Id: ktau_inject.h,v 1.1.2.3 2007/08/08 20:34:18 anataraj Exp $
 * ***************************************************************************/ 

#ifndef _KTAU_INJECT_H
#define _KTAU_INJECT_H

#include <linux/smp.h>

#define KTAU_GBLTIMER_FLAG 0
#define KTAU_LCLTIMER_FLAG 1

#define KTAU_MAX_INJECT_VALS 8

#ifdef __KERNEL__
//to hold values
extern volatile unsigned long long ktau_inject_val[KTAU_MAX_INJECT_VALS];

//to hold 'actual' flags
//One set of flags per-processor
extern volatile unsigned long long ktau_inject_flags[NR_CPUS][KTAU_MAX_INJECT_VALS];

//dummy result locations per-processor - just so that the buys loop is not optimized away!
extern volatile unsigned long long ktau_inject_dummy[NR_CPUS][KTAU_MAX_INJECT_VALS];

//a setter routine that runs on each processor, that copies new value into 'flag' for that processor
extern void ktau_inject_setter(void* info); //just sets all the flags to the values (even if they havent changed)


//a piece of inline code that busy-loops for 'flag' number of times
static inline void ktau_inject_now(int flagno) {
#ifdef CONFIG_KTAU_NOISE_INJECT
	unsigned long long lc_flag = ktau_inject_flags[smp_processor_id()][flagno];
	volatile unsigned long long a = 59, b = 43;
	while(lc_flag--) {
		a += (ktau_inject_dummy[smp_processor_id()][flagno] + (b-a) - 32);
	}
	ktau_inject_dummy[smp_processor_id()][flagno] = b+a;
#endif /* CONFIG_KTAU_NOISE_INJECT */
}

//a routine that performs the IPI
int ktau_inject_IPI(void);

//just sets 'values'
int ktau_inject_vals(unsigned long long* vals,  int num);
#endif /* __KERNEL__ */

#endif /* _KTAU_INJECT_H */
