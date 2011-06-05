/***********************************************
 * File 	: include/linux/ktau/ktau_merge.h
 * Version	: $Id: ktau_merge.h,v 1.2.2.1 2007/08/09 06:21:12 anataraj Exp $
 * ********************************************/
#ifndef _KTAU_MERGE_H_
#define _KTAU_MERGE_H_

#ifndef KTAU_USER_SRC_COMPILE
//#include <linux/config.h> //breaks 2.6.19+
#include <linux/autoconf.h>
#endif /*KTAU_USER_SRC_COMPILE*/

/*
#define KTAU_SYSCALL_MSK        1
#define KTAU_IRQ_MSK            2
#define KTAU_BH_MSK             4
#define KTAU_SCHED_MSK          8
#define KTAU_EXCEPTION_MSK      16
#define KTAU_SIGNAL_MSK         32
#define KTAU_SOCKET_MSK         64      
#define KTAU_TCP_MSK            128
#define KTAU_ICMP_MSK           256     
*/
enum ktau_merge_grps {
	merge_kernel = 0,
	merge_syscall,  
	merge_irq,
	merge_bh,
	merge_sched,
	merge_except,
	merge_signal,
	merge_socket,
	merge_tcp,
	merge_icmp,

	/* ---- all merge grps must be added above this --- */
	merge_max_grp
};


/* actual internal state within ktau_state
 * - need to find a better name */
typedef struct _ktau_state_int {
	unsigned long long 	ktime[merge_max_grp];
	unsigned long long 	kexcl[merge_max_grp];
	unsigned long long	knumcalls[merge_max_grp];
} ktau_state_int;


/*
 * This struct is the state from user-space
 * tau , used for merging.
 */
typedef struct _ktau_state {
	volatile int active_index;
	ktau_state_int state[2]; //active & inactive
} ktau_state;


//fwd decl
struct page;

typedef struct _ktau_user {
	struct page** pages;
	int nr_pages;
	unsigned long orig_kaddr;
	unsigned long uptr;

	//AN added
	ktau_state lc_state;
} ktau_user;


/*------------------------------------*/

/* API */

/*
 * fwd decl (this actually resides in ktau_hash.h)
 */
struct _ktau_prof;

/* setting & unsetting merging functionality & shared (user/kernel) data-structs */
int set_hash_mergestate(struct _ktau_prof *buf, ktau_state* pstate);

/* 
 * this function is the one that actually updates the user-visible state.
 */
int update_mergestate(ktau_user* pmerge, unsigned long long incl, unsigned long long excl, unsigned long long incr, unsigned int grp);

#endif /*_KTAU_MERGE_H_*/
