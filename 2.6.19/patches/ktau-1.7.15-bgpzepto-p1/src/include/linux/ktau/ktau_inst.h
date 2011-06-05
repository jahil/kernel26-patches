/*************************************************************************
* File 		: include/linux/ktau/ktau_inst.h
* Version	: $Id: ktau_inst.h,v 1.10.2.3 2008/11/19 05:20:47 anataraj Exp $
*************************************************************************/

#ifndef _KTAU_INST_H_
#define _KTAU_INST_H_

#include <asm/atomic.h>			/* atomic */

#include <linux/ktau/ktau_bootopt.h>

/*
 * KTAU EVENT MAPPING
 */
#define	KTAU_EVENT_SCHED		300
#define	KTAU_EVENT_SOFTIRQ		301
#define	KTAU_EVENT_TASKLET		302
#define	KTAU_EVENT_HI_TASKLET		303
#define	KTAU_EVENT_WORKQUEUE		304
#define	KTAU_EVENT_TIMER_SOFTIRQ	305
#define	KTAU_EVENT_TCP_SLOWPATH		306

#define KTAU_EXCEPTION_BASE		1055

#define GET_KTAU_INDEX()         static unsigned int ktau_index = 0;\
				 if(unlikely(!ktau_index)) ktau_index = get_ktau_index() 

#define INCR_KTAU_INDEX(incr)    static unsigned int ktau_index = 0;\
	                         if(unlikely(!ktau_index)) ktau_index = (incr_ktau_index(incr) - incr) + 1; 


//AN_20060419_OPTIMZ
extern atomic_t ktau_global_index;

/*
 * Return the global function index for hash table
 */
//extern unsigned int get_ktau_index(void);
static __inline__ unsigned int get_ktau_index(void) {
	return atomic_inc_return(&ktau_global_index);	
}

/*
 * Increment global function index by 'incr'
 * then Return the global function index for hash table
 * incremented by 1 from original value (i.e same return
 * value as from get_kau_index).
 * Note: Undefined, if incr -ve.
 */
//extern unsigned int incr_ktau_index(int incr);
static __inline__ unsigned int incr_ktau_index(int incr) {
        return atomic_add_return(incr, &ktau_global_index);
}

/*
 * The low-level timer    
 */
unsigned long long int ktau_rdtsc(void);

/* 
 * This will be used for timer type profiling.
 */
#define ktau_start_timer(x, grp)	ktau_start_prof(ktau_index, (unsigned int)x, grp)
#define ktau_stop_timer(x, grp)		ktau_stop_prof(ktau_index, (unsigned int)x, grp)

extern void _ktau_start_prof(unsigned int index, unsigned int addr, unsigned int grp); 
static __inline__ void ktau_start_prof(unsigned int index, unsigned int addr, unsigned int grp) {
#ifdef CONFIG_KTAU_BOOTOPT
	if(unlikely(ktau_verify_bootopt(grp)))
#endif /* CONFIG_KTAU_BOOTOPT*/
		_ktau_start_prof(index,addr, grp);		
}

extern void _ktau_stop_prof(unsigned int index, unsigned int addr, unsigned int grp); 
static __inline__ void ktau_stop_prof(unsigned int index, unsigned int addr, unsigned int grp) {
#ifdef CONFIG_KTAU_BOOTOPT
	if(unlikely(ktau_verify_bootopt(grp)))
#endif /*CONFIG_KTAU_BOOTOPT*/
		_ktau_stop_prof(index,addr, grp);		
}

/* 
 * This will be used for event type profiling
 */
extern void ktau_event_prof(unsigned int index, unsigned int addr);

/* For purely ONLY tracing (these dont update profile data) */
void _ktau_start_trace(unsigned int addr, unsigned int grp);
void _ktau_stop_trace(unsigned int addr, unsigned int grp);

#endif /*_KTAU_INST_H_ */
