/*****************************************************
 * File    : 	include/linux/ktau/ktau_hash.h
 * Version : 	"$Id: ktau_hash.h,v 1.11.2.12 2008/11/19 05:20:47 anataraj Exp $;
 ****************************************************/

#ifndef _KTAU_HASH_H_
#define _KTAU_HASH_H_

#define KTAU_HASH_SIZE		1536
#define KTAU_NESTED_LEVEL	20	

#include <linux/interrupt.h>

#ifdef CONFIG_SMP
#include <linux/spinlock.h>	/* For spinlock */
#endif /* CONFIG_SMP */

#include "ktau_datatype.h"

#include <linux/ktau/ktau_cont_data.h>

/* Locking and disabling  mechanism in KTAU */
#define ktau_bh_enable()        if(likely(!in_softirq()))\
                                        local_bh_enable();\
                                else\
                                        __local_bh_enable()


#if defined(CONFIG_KTAU_IRQ) || defined(CONFIG_KTAU_SCHED)
#define ktau_spin_lock(l,f)     spin_lock_irqsave(l,f)
#define ktau_spin_unlock(l,f)   spin_unlock_irqrestore(l,f)
#else /*CONFIG_KTAU_IRQ || CONFIG_KTAU_SCHED*/
#define ktau_spin_lock(l,f)     local_bh_disable();\
                                spin_lock(l)
#define ktau_spin_unlock(l,f)   spin_unlock(l);\
                                ktau_bh_enable()
#endif /*CONFIG_KTAU_IRQ || CONFIG_KTAU_SCHED*/

/*
 * this struct is used in the task_struct
 */
typedef struct _ktau_prof{
	spinlock_t 		*hash_lock;
	unsigned int 		ent_count;
	unsigned long long 	accum[KTAU_NESTED_LEVEL + 1];
	h_ent* 			ktau_hash[KTAU_HASH_SIZE];
	ktau_user		merge;
#ifdef CONFIG_KTAU_PERF_COUNTER
	unsigned int 		accum_pcounter[KTAU_MAX_PERF_COUNTER][KTAU_NESTED_LEVEL + 1];
#endif /*CONFIG_KTAU_PERF_COUNTER*/
	
#ifdef CONFIG_KTAU_TRACE
	ktau_trace* 		trbuf;
	ktau_trace* 		cur_trace ;
	ktau_trace* 		head_trace ;
	ktau_trace*		last_dump;
	ktau_trace		first_lost;
	ktau_trace		last_lost;
	//AN_20060201_TRC AN ADDED - temporarily deal this loss
	unsigned long		trace_max_no;
	unsigned long		trace_last_no;
#endif /*CONFIG_KTAU_TRACE*/

	//AN_20051102_REENT
	unsigned long long 	reent[KTAU_NESTED_LEVEL + 1];
	unsigned int 		stackDepth;

	//AN_20060419_OPTIMZ
	h_ent*			hash_mem;

	//AN_20061025_newsysc
	unsigned long		last_sysc;
	unsigned long		last_sysc_addr;

	//AN_20061105_dbginst
	unsigned long		dbgstart[KTAU_NESTED_LEVEL + 1];
	unsigned long		dbgstop[KTAU_NESTED_LEVEL + 1];

	//AN_20070228_shcontainers
	ktau_shcont 		shconts[KTAU_MAX_SHCONTS];
	int 			no_shconts;

	//AN_20070307_sh_counters
	//llist of ctrs
	struct list_head 	shctr_list;
	int 			no_shctrs;
	//one event trigger-list ptr per hash-entry
	struct list_head* 	htrigger_list[KTAU_HASH_SIZE];
	struct list_head 	pend_evs_list;
	int 			no_pend;

	int disable;
} ktau_prof; 

/*----------------------- PROFILE API -----------------------------*/
/*
 * IN APIs : These will be used in process creation
 * 	     and destruction.
 */
extern void create_task_profile(struct task_struct *tsk);
extern void remove_task_profile(struct task_struct *tsk);
extern void remove_task_profile_sched(struct task_struct *tsk);

/* GLOBAL INDEX */
extern void reset_ktau_index(void);
void ktau_global_semaphore_init(void);

/*----------------------- TRACE API -----------------------------*/
extern int trace_buf_init(struct task_struct *tsk);
//extern void* trace_buf_destroy(struct task_struct *tsk);
extern void* trace_buf_destroy(ktau_prof* prof);
extern void trace_buf_free(void* trbuf);
extern int trace_buf_write(unsigned long long tsc 
			 , unsigned long addr 
			 , unsigned long type);

//AN_20060201_TRC AN ADDED - temporarily deal this loss
int get_trace_size_self(struct task_struct*, ktau_prof *ktau, int *psize);
int get_trace_size_all(int *psize);
int get_trace_size_many(pid_t *pid_lst, unsigned int no_pids, int *psize);

int trace_buf_resize_self(struct task_struct*, ktau_prof* ktau, int newsize);
int trace_buf_resize_all(int newsize);
int trace_buf_resize_many(pid_t *pid_lst,  unsigned int no_pids, int newsize);

#endif /*_KTAU_HASH_H_*/
