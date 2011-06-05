/*************************************************************************
 * File 	: kernel/ktau/ktau_inst.c
 * Version	: $Id: ktau_inst.c,v 1.9.2.9 2008/11/19 05:26:50 anataraj Exp $
 *
 ************************************************************************/

#include <linux/kernel.h>
//#include <linux/config.h>		//breaks 2.6.19+
#include <linux/autoconf.h>
#include <linux/slab.h>			/* Kmalloc */	
#include <linux/sched.h>		/* task_struct def */
#include <linux/interrupt.h>
#include <linux/spinlock.h>        	/* spinlock */

#include <asm/atomic.h>			/* atomic */
#include <asm/current.h>		/* get_current */
#include <asm/system.h>			/* irqs_disabled */

/* For debugging message */
#define TAU_NAME "ktau_inst"
#define PFX TAU_NAME
//#define TAU_DEBUG 		1
//To generate debug messages conditonally, set the below (in additiont to setting TAU_DEBUG)
//#define TAU_DEBUG_COND		(current->pid==19)

/* KTAU header files */
#include <linux/ktau/ktau_print.h>
#include <linux/ktau/ktau_inst.h>
#include <linux/ktau/ktau_datatype.h>
#include <linux/ktau/ktau_hash.h>
#include <linux/ktau/ktau_perf.h>
#include <linux/ktau/ktau_bootopt.h>

//#define KTAU_START_INDEX	999	/* Dynamic index start at 1000 */
//Zepto Syscalls 1050+
#define KTAU_START_INDEX	1255	/* Dynamic index start at 1000 */

/*--------------------- GLOBAL INDEX  ---------------------*/

#include "ktau_linux-2.4.c"

/*
 * Global function index
 */
atomic_t ktau_global_index = ATOMIC_INIT(KTAU_START_INDEX);

/* 
 * This function is used to increment the global_ktau_index.
 */
/*
unsigned int get_ktau_index(void){
	return atomic_inc_return(&ktau_global_index);	
}
*/

/*
 * Increment global function index by 'incr'
 * then Return the global function index for hash table
 * incremented by 1 from original value (i.e same return
 * value as from get_kau_index).
 */
/*
unsigned int incr_ktau_index(int incr) {
        return atomic_add_return(incr, &ktau_global_index);
}
*/

void reset_ktau_index(void){
	atomic_set(&ktau_global_index,KTAU_START_INDEX);
}

/*--------------------------- TIMER ---------------------------------*/
/*
 * The low-level timer    
 */
#ifdef KTAU_RDTSC
#undef KTAU_RDTSC
#endif

#ifndef KTAU_RDTSC
#ifdef CONFIG_X86_64
unsigned long long int ktau_rdtsc(void)
{
     unsigned int x, y;
     __asm__ volatile ("rdtsc" : "=a" (x), "=d" (y));
     return ( ((unsigned long)x) | (((unsigned long)y) << 32) ) ;
}
#define KTAU_RDTSC
#endif /* CONFIG_x86_64 */
#endif /* KTAU_RDTSC */

#ifndef KTAU_RDTSC
#ifdef CONFIG_X86   
unsigned long long int ktau_rdtsc(void)
{
     unsigned long long int x;
     __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
     return x;
}
#define KTAU_RDTSC
#endif /* CONFIG_X86 */
#endif /* KTAU_RDTSC */

#ifndef KTAU_RDTSC
#ifdef CONFIG_PPC32
unsigned long long ktau_rdtsc(void) {

        unsigned long long time = 0;
        unsigned long lword = 0, hword = 0;

        asm volatile (
                "rdtscppc: \n\t"
                "mfspr  %%r27, 269 \n\t"
                "mfspr  %%r28, 268 \n\t"
                "mfspr  %%r29, 269 \n\t"
                "cmpw   %%r29, %%r27 \n\t"
                "bne    rdtscppc \n\t"
                "ori    %0,%%r28,0 \n\t"
                "ori    %1,%%r29,0 \n\t"
                : "=r" (lword), "=r" (hword)
                :
                : "r27", "r28", "r29"
        );

        time = hword;

        return ( (time << 32) | lword );
}
#define KTAU_RDTSC
#endif /*CONFIG_PPC32*/
#endif /* KTAU_RDTSC */

#ifndef KTAU_RDTSC
#ifdef CONFIG_MIPS
#include <asm/mipsregs.h>
unsigned long long int ktau_rdtsc(void)
{
	u32 u = jiffies;
	u32 l = read_c0_count();
	return ( (u64)(((u64)u<<32) + l ) );
}
#define KTAU_RDTSC
#endif /* CONFIG_MIPS */
#endif /* KTAU_RDTSC */
 
#ifndef KTAU_RDTSC
#error KTAU_RDTSC is NOT defined for this architecture.
#endif /* KTAU_RDTSC */

/*---------------------VERIFICATION / DEBUGGING Functions ---------------------*/
/* 
 * This function dumps a callstack of this process from KTAU_TRACE 
 * to help debugging
 */ 
void _dump_call_stack(struct task_struct *tsk, int stackSize) {
#ifdef CONFIG_KTAU_TRACE

#ifdef CONFIG_KTAU_BOOTOPT
	if(ktau_verify_bootopt(KTAU_TRACE_MSK)) {
#endif /* CONFIG_KTAU_BOOTOPT*/
		int i;
		ktau_trace *c = tsk->ktau->cur_trace;
		ktau_trace *h = tsk->ktau->head_trace;
		ktau_trace *t = tsk->ktau->trbuf;
		//ktau_trace *l = tsk->ktau->last_dump;

		printk("KTAU_TRACE STACK:\n");
		for(i=0;i<=stackSize && c!=h ;i++){
			if(c > t)
				c--;
			else
				c = (t + KTAU_TRACE_LAST);
			
			printk("tsc  :%20llu, ",c->tsc);
			printk("addr :%15x, ",c->addr);
			printk("type :%5u\n",c->type);
		}
#ifdef CONFIG_KTAU_BOOTOPT
	}
#endif /* CONFIG_KTAU_BOOTOPT*/

#endif /*CONFIG_KTAU_TRACE */
}

void dump_call_stack(int stackSize) {
	_dump_call_stack(current, stackSize);
}


/*
 * This function verifies that the inclusive > exclusive time.
 */
int _verify_excl_incl(struct task_struct *tsk, unsigned long long excl, unsigned long long incl, 
		unsigned long long diff_time, h_ent* cur_ent)
{
		if(excl > incl){
			printk("verify_excl_incl: Exclusive time is greater than inclusive time.\nPID  = %d\nExcl = %llu\nIncl = %llu\n",
				tsk->pid,excl, incl);
		
			printk("in function = %x\n",cur_ent->addr);
			printk("stackDepth = %d\n",tsk->ktau->stackDepth);
			printk("nestingLevel = %d\n",cur_ent->nestingLevel);
			printk("diff_time = %llu\n",diff_time);
			_dump_call_stack(tsk, 24);
			return(-1);
		}
		return(0);
}
int verify_excl_incl(unsigned long long excl, unsigned long long incl, 
		unsigned long long diff_time, h_ent* cur_ent) {
	return _verify_excl_incl(current, excl, incl, diff_time, cur_ent);
}


void show_dbg_startstop(char* msg, struct task_struct* tsk, unsigned long addr) {
#ifdef CONFIG_KTAU_DEBUG
	int level = 0;
	printk("KTAU_DBG: %s\n", msg);
	printk("in function = %lx\n",addr);
	printk("stackDepth = %d\n",tsk->ktau->stackDepth);
	for(level = 0; level <= tsk->ktau->stackDepth; level++) {
		printk("start[%d] : %lx  | stop[%d] : %lx\n",
		level, tsk->ktau->dbgstart[level], tsk->ktau->stackDepth-level, 
		tsk->ktau->dbgstop[tsk->ktau->stackDepth-level]);
	}
#endif /*CONFIG_KTAU_DEBUG*/
}

/*---------------------KTAU INSTRUMENTATION ---------------------*/
/*
 * This fuction is the start ONLY TRACE
 */
void __ktau_start_trace(struct task_struct* tsk, unsigned int addr, unsigned int grp){
	unsigned long flags = 0;

	ktau_spin_lock(&tsk->ktau_lock,flags);

	if(!tsk->ktau) {
		goto out_unlock;
	}

	/********************************************************/
#ifdef CONFIG_KTAU_TRACE
#ifdef CONFIG_KTAU_BOOTOPT
	if(unlikely(ktau_verify_bootopt(KTAU_TRACE_MSK)))
#endif /* CONFIG_KTAU_BOOTOPT*/
		trace_buf_write(ktau_rdtsc(), addr, KTAU_TRACE_START);
#endif /*CONFIG_KTAU_TRACE*/
	/********************************************************/
out_unlock:
	ktau_spin_unlock(&tsk->ktau_lock,flags);
}
void _ktau_start_trace(unsigned int addr, unsigned int grp){
	__ktau_start_trace(current, addr, grp);
}

/*
 * This fuction is the stop ONLY TRACE
 */
void __ktau_stop_trace(struct task_struct* tsk, unsigned int addr, unsigned int grp){
	unsigned long flags = 0;

	if(!tsk->ktau) return;

	ktau_spin_lock(&tsk->ktau_lock,flags);
	/********************************************************/
#ifdef CONFIG_KTAU_TRACE
#ifdef CONFIG_KTAU_BOOTOPT
	if(unlikely(ktau_verify_bootopt(KTAU_TRACE_MSK)))
#endif /* CONFIG_KTAU_BOOTOPT*/
		trace_buf_write(ktau_rdtsc(), addr, KTAU_TRACE_STOP);
#endif /*CONFIG_KTAU_TRACE*/
	/********************************************************/
	ktau_spin_unlock(&tsk->ktau_lock,flags);
}
void _ktau_stop_trace(unsigned int addr, unsigned int grp) {
	__ktau_stop_trace(current, addr, grp);
}

//decl
extern int ktau_reg_single_ev(struct task_struct* tsk, int index, ktau_trigger* new_ev, unsigned long* pirqflags);

/* process_pend_triggers:
 * check and register handler/triggers that may be interested in this "new"
 * instrumentation.point.
 * We need to do this as currently in KTAU instrumentation point IDs are
 * dynamically generated when the instr.pt is encountered the 1st time.
 * So at time of event registration, if event has not occured, its ID may
 * be unknown.
 * 
 * tsk:		struct task_struct
 * cur_ent:	h_ent* from ktau_hash for this instr.pt
 * index:	the index of ktau_hash - i.e. the ID of instr.pt
 * grp:		grp to which this instr.pt belongs to
 * pkflags:	ptr to kflags - the stack variable in caller used for spinlock/unlock
 * Return:
 * On Success, +ve Number of pending triggers sucessfully registred.
	- if no triggers are processed at all - then 0.
 * On Failure, -ve.
 */
static inline int process_pend_triggers(struct task_struct* tsk, h_ent* cur_ent, int index, unsigned int grp, unsigned long* pirqflags) {
	struct list_head* p = NULL, *tmp = NULL;
	ktau_trigger* trig = NULL;
	int ret = 0;

	list_for_each_safe(p, tmp, &(tsk->ktau->pend_evs_list)) {
		trig = list_entry(p, ktau_trigger, list);	
		//check if it matches
		if(cur_ent->addr != (unsigned int)trig->sym) {
			continue;
		}
		//got it - delete from pend_list, decr no_pends
		list_del(p);
		tsk->ktau->no_pend--;

		//add it to cur_ent's trigger_list
		if(ktau_reg_single_ev(tsk, index, trig, pirqflags) < 0) {
			//how to handle error?
			err("process_pend_triggers: ktau_reg_single_ev failed. Now not in pend or list.");
			continue; //as there may be other triggers...
		}

		ret++;
	}

	return ret;
}


/*
 * This fuction is the start profile
 */
void __ktau_start_prof(struct task_struct* tsk, unsigned int index,unsigned int addr, unsigned int grp){
	unsigned long flags = 0;
	h_ent *cur_ent;

	ktau_spin_lock(&tsk->ktau_lock,flags);

	if(!tsk->ktau) goto ktau_unlock;

	if(tsk->ktau->disable == 1) goto ktau_unlock;

	info("_ktau_start_prof: Acquired spin-lock. Index:%u Addr:%x\n", index, addr);
	
	/********************************************************/
#ifdef CONFIG_KTAU_TRACE
#ifdef CONFIG_KTAU_BOOTOPT
	if(unlikely(ktau_verify_bootopt(KTAU_TRACE_MSK)))
#endif /* CONFIG_KTAU_BOOTOPT*/
		trace_buf_write(ktau_rdtsc(), addr, KTAU_TRACE_START);
#endif /*CONFIG_KTAU_TRACE*/
	/********************************************************/

#ifndef CONFIG_KTAU_TRACE
	cur_ent = (h_ent*)(tsk->ktau->ktau_hash[index]);
	/*
	 * Update hash table
	 */
	if(unlikely(!(cur_ent))) {
		//AN_20060419_OPTIMZ
		//tsk->ktau->ktau_hash[index] = kmalloc(sizeof(h_ent),GFP_ATOMIC);
		info("_ktau_start_prof: cur_ent == NULL.\n");
		if(!tsk->ktau->hash_mem) { 
			tsk->ktau->disable = 1;
			goto ktau_unlock;
		}
		tsk->ktau->ktau_hash[index] = (tsk->ktau->hash_mem+index);
		tsk->ktau->ent_count++;
		cur_ent = tsk->ktau->ktau_hash[index];
		cur_ent->type 			= KTAU_TIMER;
		cur_ent->data.timer.count 	= 0;
		cur_ent->data.timer.incl 	= 0;
		cur_ent->data.timer.excl 	= 0;
		cur_ent->addr 	 		= addr;
		cur_ent->nestingLevel		= 0;

#ifdef CONFIG_KTAU_MERGE
		//check for pending triggers
		// - there may be some events that include this instr.pt
		process_pend_triggers(tsk, cur_ent, index, grp, &flags);
#endif /*CONFIG_KTAU_MERGE*/

	}

	info("_ktau_start_prof: cur_ent != NULL.\n");

	/*********************************Simple Recursive START logic*******************************/
	/* In this version, we make every function to be reentrantable by keeping multiple
	 * timer.start in an array call reent. This array is used in all cases.
	 */

	// 0. Checking if it is exceeding the stackDepth.
	// AN_20060419_OPTIMZ
	if(tsk->ktau->stackDepth >= (KTAU_NESTED_LEVEL-1)){
#ifdef CONFIG_KTAU_DEBUG
		err("ktau_start_prof: Exceeding stackDepth allowed:\nPID:  %d, stackDepth: %u, addr: %x, index: %u\n",
				tsk->pid,
				tsk->ktau->stackDepth,
				addr,
				index);
		dump_call_stack(24);
		show_dbg_startstop("ktau_start", tsk, addr);
#endif /*CONFIG_KTAU_DEBUG*/
		tsk->ktau->disable = 1;
		goto ktau_unlock;
	}

	info("_ktau_start_prof: stackDepth(%d) is valid.\n", tsk->ktau->stackDepth);

#ifdef CONFIG_KTAU_DEBUG
	tsk->ktau->dbgstart[tsk->ktau->stackDepth] = addr;
#endif /*CONFIG_KTAU_DEBUG*/

	// 1. get the start time
	if(likely(tsk->ktau->reent[tsk->ktau->stackDepth] == 0)) {
		info("_ktau_start_prof: Reading rdtsc...\n");
		tsk->ktau->reent[tsk->ktau->stackDepth] = ktau_rdtsc();

		info("ktau_start_prof: kernel function= %x, Level= %3u, PID = %d"
				,cur_ent->addr,cur_ent->nestingLevel,tsk->pid);
	}
	else {
		/* If the start time is not zero, there is something wrong with the logic. */	
		err("ktau_start_prof: ERROR in kernel profiling logic. PID=%d addr=%x index=%d stackDepth=%d nestingLevel=%d\n",
				tsk->pid, addr, index, tsk->ktau->stackDepth, 
				cur_ent->nestingLevel);

		dump_call_stack(24);
		show_dbg_startstop("ktau_start", tsk, addr);
		tsk->ktau->disable = 1;
		goto ktau_unlock;
	}

	// 2. Now increment stackDepth
	tsk->ktau->stackDepth++;

	/* 3. Increment this function's nestingLevel. This is to seperately track 
	 *    only this function's stack.
	 */
	cur_ent->nestingLevel++;

	info("_ktau_start_prof: Incremented stackDepth(%d) & nestingLevel(%d).\n", tsk->ktau->stackDepth, cur_ent->nestingLevel);

	/**************************************** END of Start Logic ******************************/
#endif /* ifndef CONFIG_KTAU_TRACE */
ktau_unlock:
	ktau_spin_unlock(&tsk->ktau_lock,flags);

	info("_ktau_start_prof: spin_unlock-ed. Index:%u Addr:%x\n", index, addr);
}
void _ktau_start_prof(unsigned int index,unsigned int addr, unsigned int grp) {
	__ktau_start_prof(current, index, addr, grp);
}


/* process_triggers:
 * check and execute any handlers for registred events
 * 
 * tsk:		struct task_struct
 * type:	the type of event KTAU_TRIGGER_TYPE_START or STOP
 * index:	the index of ktau_hash - i.e. the ID of instr.pt
 * incl:	incl time
 * excl:	excl time
 * grp:		grp to which this instr.pt belongs to
 *
 * Return:
 * On Success, +ve Number of triggers processed.
	- if no triggers are processed at all - then 0.
 * On Failure, -ve index of trigger that failed + 1. 
 *	- if 1st trigger fails, then it returns -1;
 *	- if 3rd trigger fails, then returns -3;
 */
static inline int process_triggers(struct task_struct* tsk, int type, int index, unsigned long long incl, unsigned long long excl, unsigned int grp) {
	struct list_head* p = NULL;
	ktau_trigger* trig = NULL;
	int ret = 0;

	info("process_triggers: current:%d tsk-pid:%d type:%d index:%d grp:%u\n", current->pid, tsk->pid, type, index, grp);

	list_for_each(p, (tsk->ktau->htrigger_list[index])) {
		ret++;
		trig = list_entry(p, ktau_trigger, list);	
		if((*(trig->handler))(trig, type, incl, excl)) {
			err("process_triggers: Exit: ERROR current:%d tsk-pid:%d type:%d index:%d grp:%u\n", current->pid, tsk->pid, type, index, grp);
			return -ret;
		}
	}

	info("process_triggers: Exit: current:%d tsk-pid:%d type:%d index:%d grp:%u\n", current->pid, tsk->pid, type, index, grp);

	return ret;
}

/*
 * This fuction is the wrapper for the _ktau_start_prof
 */
//void ktau_start_prof(unsigned int index,unsigned int addr,unsigned int grp){
//#ifdef CONFIG_KTAU_BOOTOPT
//	if(ktau_verify_bootopt(grp))
//#endif /* CONFIG_KTAU_BOOTOPT*/
//		_ktau_start_prof(index,addr, grp);		
//}


static inline unsigned long long ktau_time_diff(unsigned long long now, unsigned long long prev) {
#ifdef CONFIG_MIPS	
	//wrapover is number of jiffies for 32-bit ctr to wrap given SiCo MHz
	//TODO: Calibrate automatically, and cleanup this routine
	#define CWRAPOVER (17179)
	u32 jnow = ((now >> 32) & ((u64)0xFFFFFFFF << 32));
	u32 jprev = ((prev >> 32) & ((u64)0xFFFFFFFF << 32));
	u32 cnow = (now & 0xFFFFFFFF);
	u32 cprev = (prev & 0xFFFFFFFF);
	u32 jdiff = jnow - jprev;
	int nowraps = 0;
	//does cycle ctr has all we need	
	if(jdiff <= CWRAPOVER) {
		//case 1: 0 wrap-over
		if(cnow > cprev) {
			return (cnow - cprev);
		}
		//case 2: single wrap-over
		return ( (((u64)0xFFFFFFFF) - cprev) + cnow);
	} 
	//other-wise ... we need both jiffies_diff and cycle_diff
	//figure out how many wrap-overs occured..., then use like
	//case 2 above.
	nowraps = jdiff / CWRAPOVER;
	return ( ((((u64)0xFFFFFFFF) * nowraps) - cprev) + cnow);
#else //CONFIG_MIPS for all others , simply subtract
 	return (now - prev);
#endif
}


void __ktau_stop_prof(struct task_struct* tsk, unsigned int index, unsigned int addr, unsigned int grp) {
	unsigned long flags = 0;
	unsigned long long diff_time = 0;
#ifndef CONFIG_KTAU_TRACE
	unsigned long long lc_excltime = 0;
	h_ent *cur_ent = NULL;
#endif /*CONFIG_KTAU_TRACE*/

	ktau_spin_lock(&tsk->ktau_lock,flags);

	if(!tsk->ktau) goto out_unlock;

	if(tsk->ktau->disable == 1) goto out_unlock;

	info("_ktau_stop_prof: Acquired spin-lock. Index:%u Addr:%x\n", index, addr);

	/********************************************************/
	// read the STOP time
	diff_time = ktau_rdtsc();


#ifdef CONFIG_KTAU_TRACE
#ifdef CONFIG_KTAU_BOOTOPT
	if(unlikely(ktau_verify_bootopt(KTAU_TRACE_MSK)))
#endif /* CONFIG_KTAU_BOOTOPT*/
		trace_buf_write(diff_time, addr, KTAU_TRACE_STOP);
#endif /*CONFIG_KTAU_TRACE*/


#ifndef CONFIG_KTAU_TRACE
	cur_ent = (h_ent*)(tsk->ktau->ktau_hash[index]);
	if(unlikely(!(cur_ent))) {
		err("ktau_stop_prof: Error. Stopping with no start. index = %u, addr = %x\n",
				index, addr);	
		goto out_unlock;
	}

	/***************************Simple Recursive Incl/Excl Time Logic******************/

	if(tsk->ktau->stackDepth > KTAU_NESTED_LEVEL){
#ifdef CONFIG_KTAU_DEBUG
		err("ktau_start_prof: Exceeding stackDepth allowed:\nPID:  %d, stackDepth: %u, addr: %x, index: %u\n",
				tsk->pid,
				tsk->ktau->stackDepth,
				addr,
				index);
#endif /*CONFIG_KTAU_DEBUG*/
		tsk->ktau->disable = 1;
		goto out_unlock;
	}

	// 1. --- Decrement StackDepth - but check before doing so
	if(tsk->ktau->stackDepth <= 0) {
#ifdef CONFIG_KTAU_DEBUG
		err("ktau_start_prof: stackDepth going to -ve:\nPID:  %d, stackDepth: %u, addr: %x, index: %u\n",
				tsk->pid,
				tsk->ktau->stackDepth,
				addr,
				index);
#endif /*CONFIG_KTAU_DEBUG*/
		tsk->ktau->disable = 1;
		goto out_unlock;
	}

	info("_ktau_stop_prof: stackDepth(%d) before dec is valid.\n", tsk->ktau->stackDepth);

	tsk->ktau->stackDepth--;

#ifdef CONFIG_KTAU_DEBUG
	tsk->ktau->dbgstop[tsk->ktau->stackDepth] = addr;
	if(addr != tsk->ktau->dbgstart[tsk->ktau->stackDepth]) {
		show_dbg_startstop("ktau_stop", tsk, addr);
	}
#endif /*CONFIG_KTAU_DEBUG*/

	// 2. --- Decrement the nesting level - but check first
	if(cur_ent->nestingLevel == 0) {
#ifdef CONFIG_KTAU_DEBUG
		err("ktau_start_prof: nestingLevel going to -ve:\nPID:  %d, stackDepth: %u, addr: %x, index: %u\n",
				tsk->pid,
				tsk->ktau->stackDepth,
				addr,
				index);
#endif /*CONFIG_KTAU_DEBUG*/
		tsk->ktau->disable = 1;
		goto out_unlock;
	}

	info("_ktau_stop_prof: nestingLevel(%d) before dec is valid.\n", cur_ent->nestingLevel);

	cur_ent->nestingLevel--;

	info("_ktau_stop_prof: Decremented stackDepth(%d) & nestingLevel(%d).\n", tsk->ktau->stackDepth, cur_ent->nestingLevel);

	// 3. --- Increment Call-count
	cur_ent->data.timer.count++;

	// 4. --- Calc incl time and store in "diff_time"
	// We calc-incl time using reent[] array. 
	//diff_time = diff_time - tsk->ktau->reent[tsk->ktau->stackDepth];
 	diff_time = ktau_time_diff(diff_time, tsk->ktau->reent[tsk->ktau->stackDepth]);

	/* set that array[index] value to ZERO for future use. */
	tsk->ktau->reent[tsk->ktau->stackDepth] = 0;

	// 5. --- Add that incl time to Total incl time of function
	if(likely(cur_ent->nestingLevel == 0)) {
		/* Incl-time of re-ent/recursive functions must be ADDED only ONCE *
		 * i.e. the incl-time of the outer-most invocation of that func    *
		 * is used. The inc-times of other child-reent calls of the same   *
		 * function are included in this incl-time. */
		cur_ent->data.timer.incl += diff_time;
	}

	// 6. --- Help parent-excl time calc: add our incl to accum[] at tsk stackDepth
	tsk->ktau->accum[tsk->ktau->stackDepth] += diff_time; 
	
	// 7. --- Calc our excl time: our incl - accum-incl of children seen : AND add it to tot func. excl time
        lc_excltime = (diff_time - tsk->ktau->accum[tsk->ktau->stackDepth+1]);
        cur_ent->data.timer.excl += lc_excltime;

	// 8. --- After use, Reset accum-child incl time to ZERO, so that it can be used next time
	tsk->ktau->accum[tsk->ktau->stackDepth+1] = 0;

#ifdef CONFIG_KTAU_MERGE
        /* Update ktau_state at every level */
        if(unlikely(tsk->ktau->merge.uptr)) {
                if(tsk->ktau->stackDepth == 0) {
                        update_mergestate(&(tsk->ktau->merge), diff_time, diff_time, 1 /* incr by 1 */, 0 /*merge_kernel*/);
                }
                if(cur_ent->nestingLevel == 0) {
                        //then merge incl and excl. otherwise only excl time
                        update_mergestate(&(tsk->ktau->merge), diff_time, lc_excltime, 1 /*incr by 1*/, grp);
                } else {
                        update_mergestate(&(tsk->ktau->merge), 0, lc_excltime, 1 /*incr by 1*/, grp);
                }
        }
	//trigger processing code here - for every trigger call the event handler for shared counters
	//check if processing req. TODO: Check and handle return err/success...
	if(tsk->ktau->htrigger_list[index]) {
		info("_ktau_stop_prof: current:%d tsk-pid:%d index:%d  Check-NestLevel and call process_triggers...\n", current->pid, tsk->pid, index);
                if(cur_ent->nestingLevel == 0) {
			process_triggers(tsk, KTAU_TRIGGER_TYPE_STOP, index, diff_time/*incl*/, lc_excltime, grp);
		} else {
			process_triggers(tsk, KTAU_TRIGGER_TYPE_STOP, index, 0/*incl*/, lc_excltime, grp);
		}
	}
	
	
#endif /*CONFIG_KTAU_MERGE*/

	/* Eventually, we have to verify that inclusive time >= exclusive time */
#ifdef CONFIG_KTAU_DEBUG
	if(cur_ent->nestingLevel == 0){
		verify_excl_incl(cur_ent->data.timer.excl, cur_ent->data.timer.incl, diff_time, cur_ent); 
	}
#endif /*CONFIG_KTAU_DEBUG*/

#endif /* ifndef CONFIG_KTAU_TRACE */
out_unlock:
	ktau_spin_unlock(&tsk->ktau_lock,flags);

	info("_ktau_stop_prof: spin_unlock-ed. Index:%u Addr:%x\n", index, addr);
}
void _ktau_stop_prof(unsigned int index, unsigned int addr, unsigned int grp) {
	__ktau_stop_prof(current, index, addr, grp);
}


/*
 * This fuction is the wrapper for the _ktau_stop_prof
 */
//void ktau_stop_prof(unsigned int index,unsigned int addr,unsigned int grp){
//#ifdef CONFIG_KTAU_BOOTOPT
//	if(ktau_verify_bootopt(grp))
//#endif /*CONFIG_KTAU_BOOTOPT*/
//		_ktau_stop_prof(index,addr, grp);		
//}

////////////////////// still in dev ........ ////////////////////
#ifdef CONFIG_KTAU_SCHEDSM
extern int schedule_vol(void);
extern int schedule_yld(void);
extern int schedule_pre(void);
extern asmlinkage void __sched schedule(void);
static int sched_index = -1, schedvol_index = -1, schedyld_index = -1, schedpre_index = -1;
#endif // CONFIG_KTAU_SCHEDSM

static int ktau_sm_first = 0;

void ktau_enter_sm(struct task_struct* tsk, int state) {
#ifdef CONFIG_KTAU_SCHEDSM
	if( (!tsk->pid) || (!tsk->ktau)) {
		return;
	}
	if(ktau_sm_first == 0) {
		ktau_sm_first = 1;
		sched_index = (incr_ktau_index(4) - 4) + 1; 
		schedvol_index = sched_index + 1;
		schedyld_index = sched_index + 2;
		schedpre_index = sched_index + 3;
	}

	if(tsk->ktau->disable == 1) return;

	if((tsk->ktau->curstate == KTAU_YIELD) && (state != KTAU_YIELD)) {
		return;
	}

	switch(state) {
		case KTAU_NONE:
			if(tsk->pid % 9999 == 0) {
				err("ktau_enter_sm: pid:%d bad state KTAU_NONE - ignoring.", tsk->pid);
			} else {
				info("ktau_enter_sm: pid:%d bad state KTAU_NONE - ignoring.", tsk->pid);
			}
			break;

		case KTAU_YIELD:
			err("ktau_enter_sm: pid:%d Yield-state.", tsk->pid);
			//} else {
			//	info("ktau_enter_sm: pid:%d Yield-state.", tsk->pid);
			//}
			tsk->ktau->prevstate = tsk->ktau->curstate;
			tsk->ktau->curstate = KTAU_YIELD;
			
			__ktau_start_prof(tsk, schedyld_index, (unsigned int)&schedule_yld, KTAU_SCHED_MSK);
			break;

		case KTAU_ANYSCHED:
			if(tsk->pid % 9999 == 0) {
				err("ktau_enter_sm: pid:%d AnySched-state.", tsk->pid);
			} else {
				info("ktau_enter_sm: pid:%d AnySched-state.", tsk->pid);
			}
			//need to decide if we are pre or vol
			if(tsk->state == TASK_RUNNING) {
				//means we got preempted?
				tsk->ktau->prevstate = tsk->ktau->curstate;
				tsk->ktau->curstate = KTAU_PRUN;
				
				__ktau_start_prof(tsk, sched_index, (unsigned int)&schedule, KTAU_SCHED_MSK);
			} else {
				//means we are waiting for something, io?
				tsk->ktau->prevstate = tsk->ktau->curstate;
				tsk->ktau->curstate = KTAU_IOWAIT;
				
				__ktau_start_prof(tsk, schedvol_index, (unsigned int)&schedule_vol, KTAU_SCHED_MSK);
			}
			break;
		case KTAU_CHG_RUN:
			//} else {
			//	info("ktau_enter_sm: pid:%d ChgRun-state.", tsk->pid);
			//}
			if(tsk->ktau->curstate == KTAU_IOWAIT) {
				err("ktau_enter_sm: cur:%d pid:%d ChgRun-state (iowait to running).", current->pid, tsk->pid);
				__ktau_stop_prof(tsk, schedvol_index, (unsigned int)&schedule_vol, KTAU_SCHED_MSK);
				tsk->ktau->prevstate = tsk->ktau->curstate;
				//if this tsk is current, then its already running - no need to start a pre-empt sched timer.
				//otherwise start the timer.
				if(tsk != current) {
					__ktau_start_prof(tsk, schedpre_index, (unsigned int)&schedule_pre, KTAU_SCHED_MSK);
					tsk->ktau->curstate = KTAU_P2RUN;
				} else {
					tsk->ktau->curstate = KTAU_RUN;
				}
			} else {
				err("ktau_enter_sm: cur:%d pid:%d ChgRun-state (XXX to running).", current->pid, tsk->pid);
			}
			
			break;
		default:
			err("ktau_enter_sm: unknown state (%d) - ignoring.", state);
			break;
	}
	
#endif // CONFIG_KTAU_SCHEDSM
	
	return ;
}
EXPORT_SYMBOL(ktau_enter_sm);

void ktau_exit_sm(struct task_struct* tsk, int state) {
#ifdef CONFIG_KTAU_SCHEDSM
	if(ktau_sm_first == 0) {
		return;
	}

	if( (!tsk->pid) || (!tsk->ktau)) {
		return;
	}

	if(tsk->ktau->disable == 1) return;

	if((tsk->ktau->curstate == KTAU_YIELD) && (state != KTAU_YIELD)) {
		return;
	}

	switch(state) {
		case KTAU_NONE:
			if(tsk->pid % 9999 == 0) {
				err("ktau_exit_sm: pid:%d bad state KTAU_NONE - ignoring.", tsk->pid);
			} else {
				info("ktau_exit_sm: pid:%d bad state KTAU_NONE - ignoring.", tsk->pid);
			}
			break;

		case KTAU_YIELD:
			err("ktau_exit_sm: pid:%d Yield-state.", tsk->pid);
			//} else {
			//	info("ktau_exit_sm: pid:%d Yield-state.", tsk->pid);
			//}
			__ktau_stop_prof(tsk, schedyld_index, (unsigned int)&schedule_yld, KTAU_SCHED_MSK);
			tsk->ktau->prevstate = tsk->ktau->curstate;
			tsk->ktau->curstate = KTAU_RUN;
			break;

		case KTAU_ANYSCHED:
			if(tsk->pid % 9999 == 0) {
				err("ktau_exit_sm: AnySched-state. pid:%d\n", tsk->pid);
			} else {
				info("ktau_exit_sm: AnySched-state. pid:%d\n", tsk->pid);
			}
			//need to decide if we are pre or vol
			if(tsk->ktau->curstate == KTAU_PRUN) {
				if(tsk->pid % 9999 == 0) {
					err("ktau_exit_sm: AnySched-state : pid: %d  cur-state:PRUN .", tsk->pid);
				} else {
					info("ktau_exit_sm: AnySched-state : pid: %d  cur-state:PRUN .", tsk->pid);
				}
				//means we got preempted earlier , but are getting back the cpu?
				tsk->ktau->prevstate = tsk->ktau->curstate;
				tsk->ktau->curstate = KTAU_RUN;
				
				__ktau_stop_prof(tsk, sched_index, (unsigned int)&schedule, KTAU_SCHED_MSK);
			} else if(tsk->ktau->curstate == KTAU_IOWAIT) {
				//means we were waiting voluntarily for something, io and we got back the cpu since stuff arrived?
				if(tsk->pid % 9999 == 0) {
					err("ktau_exit_sm: AnySched-state : pid: %d  cur-state:YIELD prev-state (%d) != KTAU_PRUN (%d).", tsk->pid, tsk->ktau->curstate, KTAU_PRUN);
				} else {
					info("ktau_exit_sm: AnySched-state : pid: %d  cur-state: YIELD prev-state (%d) != KTAU_PRUN (%d).", tsk->pid, tsk->ktau->curstate, KTAU_PRUN);
				}
				tsk->ktau->prevstate = tsk->ktau->curstate;
				tsk->ktau->curstate = KTAU_RUN;
				
				__ktau_stop_prof(tsk, schedvol_index, (unsigned int)&schedule_vol, KTAU_SCHED_MSK);
			} else if(tsk->ktau->curstate == KTAU_P2RUN) {
				//means we got preempted - but special case:
				//we were waiting for I/O. It arrivied and ready. But then we couldnt run since somebody else was.
				//if(tsk->pid % 9999 == 0) {
					err("ktau_exit_sm: AnySched-state P2RUN to Running: pid: %d  .", tsk->pid);
				//} else {
				//	info("ktau_exit_sm: AnySched-state : pid: %d  cur-state:P2RUN .", tsk->pid);
				//}
				tsk->ktau->prevstate = tsk->ktau->curstate;
				tsk->ktau->curstate = KTAU_RUN;
				
				__ktau_stop_prof(tsk, schedpre_index, (unsigned int)&schedule_pre, KTAU_SCHED_MSK);
			}
			break;

		default:
			err("ktau_exit_sm: pid:%d bad state (%d) - ignoring.", tsk->pid, state);
			break;
	}
#endif // CONFIG_KTAU_SCHEDSM

	return;
}
EXPORT_SYMBOL(ktau_exit_sm);


/*
 * This fuction is the event profile
 */
void ktau_event_prof(unsigned int index, unsigned int addr) {
	/*
	unsigned long flags = 0;
	h_ent *cur_ent = (h_ent*)(current->ktau->ktau_hash[index]);

	ktau_spin_lock(&current->ktau_lock,flags);

	if(!(cur_ent)){
		current->ktau->ktau_hash[index] = kmalloc(sizeof(h_ent),GFP_ATOMIC);
		current->ktau->ent_count++;
		cur_ent = current->ktau->ktau_hash[index];
		cur_ent->type = KTAU_EVENT;
		cur_ent->data.timer.count = 0;
		cur_ent->data.timer.incl = 0;
		//cur_ent->data.timer.addr  = addr;
	}
	cur_ent->data.timer.count++;

	ktau_spin_unlock(&current->ktau_lock,flags);
	*/
}
