/*************************************************************
 * File 	: kernel/ktau/ktau_buffer.c
 * Version 	: $Id: ktau_buffer.c,v 1.3.2.6 2008/11/19 05:26:50 anataraj Exp $
 *
 ************************************************************/

#include <linux/kernel.h>
//#include <linux/config.h> //breaks 2.6.19+
#include <linux/autoconf.h>
#include <linux/init.h>
#include <linux/linkage.h>      /* system call stuff */
#include <linux/sched.h>        /* task_struct def. */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/vmalloc.h>	/* vmalloc() */
#include <linux/interrupt.h>
#include <linux/spinlock.h>	/* spinlock */
#include <linux/rwsem.h>	/* R/W semaphore */

#include <asm/current.h>	/* get_current */
#include <asm/uaccess.h>

#ifdef CONFIG_KTAU_TRACE

#define TAU_NAME "ktau_buffer"
#define PFX TAU_NAME
//#define TAU_DEBUG		/* to turn on debug messages */
#include <linux/ktau/ktau_print.h>
#include <linux/ktau/ktau_hash.h>

#include "ktau_linux-2.4.c"

extern struct rw_semaphore ktau_global_sem;

static inline ktau_trace* __trace_buf_alloc(int no_entries) {
	return vmalloc(sizeof(ktau_trace)*no_entries);
}

void trace_buf_free(void* ptr) {
	vfree(ptr);
}

static int __trace_buf_init(ktau_prof* ktau, int no_entries, ktau_trace* alloced_buf) {
	//AN_20060201_TRC
	ktau->trace_max_no = no_entries;
	ktau->trace_last_no = no_entries - 1; //(or trace_max_no - 1)

	/* Allocate circular buffer */	
	//AN_20060201_TRC
	if(!(ktau->trbuf = alloced_buf)) { 
	//if(!(tsk->ktau->trbuf = vmalloc(sizeof(ktau_trace)*KTAU_TRACE_MAX))){
	
	//if(!(tsk->ktau->trbuf = kmalloc(sizeof(ktau_trace)*KTAU_TRACE_MAX,
	//		GFP_KERNEL))){
		if(no_entries > 0) {
		  err("__trace_buf_init: Null memory for trbuf (asked for:%d entries).", no_entries);
                } else {
		  info("__trace_buf_init: Null memory for trbuf.");
                }
		return(-1);
	}
	
	/* Initialize pointers */
	ktau->head_trace 	= ktau->trbuf;
	ktau->cur_trace 	= ktau->trbuf;
	ktau->last_dump 	= ktau->trbuf;
	/* Initialize trace lost start */	
	ktau->first_lost.tsc	= 0;
	ktau->first_lost.addr	= 0;
	ktau->first_lost.type	= KTAU_TRACE_START;
	/* Initialize trace lost stop */	
	ktau->last_lost.tsc	= 0;
	ktau->last_lost.addr	= 0;
	ktau->last_lost.type	= KTAU_TRACE_STOP;

	return 0;
}

int trace_buf_init(struct task_struct *tsk){
	if(tsk->ktau) {
		if(!tsk->ktau->trbuf){
			return __trace_buf_init(tsk->ktau, KTAU_TRACE_MAX, __trace_buf_alloc(KTAU_TRACE_MAX));
		}
	}
	return(0);
}

/*
 * Note: 
 * 	- cur_trace always points to the next available slot in 
 * 	the buffer.
 * 	- This function should be called inside the local spinlock 
 */
int trace_buf_write(unsigned long long tsc
		, unsigned long addr
		, unsigned long type)
{
	ktau_trace *c, *h, *t, *l;
	unsigned long lc_last_no = 0;

	if((!(current->pid)) || (!(current->ktau)) || (!(current->ktau->trbuf)) ) {
		return 0;
	}

	c = current->ktau->cur_trace;
	h = current->ktau->head_trace;
	t = current->ktau->trbuf;
	l = current->ktau->last_dump;

	if(current->pid) {
		lc_last_no = current->ktau->trace_last_no;

		if(t && c){
			/* Store trace data*/
			c->tsc  = tsc;
			c->addr = addr;
			c->type = type;
				
			/* Update current/head pointer */
			if(c==t && h==t){
				/* First case */
				current->ktau->cur_trace++;   	
			}else if(c > h ){ 
				/* Not full case */
				//if(c < (t + KTAU_TRACE_LAST)){
				//AN_20060201_TRC
				if(c < (t + lc_last_no)){
					/* Not last entry */
					current->ktau->cur_trace++;  
				}else{
					/* Current is at the last entry, wrap over*/
					current->ktau->cur_trace 	= t;
					if(l == h){
						/* First we have to keep track of the period of time
						 * that the trace event are lost 
						 */
						if(current->ktau->first_lost.tsc == 0 &&
						   current->ktau->last_lost.tsc == 0){
							current->ktau->first_lost.tsc = current->ktau->last_dump->tsc;
							current->ktau->last_lost.tsc  = current->ktau->last_dump->tsc; 
						}else {
							current->ktau->last_lost.tsc  = current->ktau->last_dump->tsc; 
						}
						current->ktau->last_dump = (t+1);
						//printk("Warning! KTAU: trace_buf_write: Losing trace info.\n");
					}
					current->ktau->head_trace 	= (t+1);
				}
			}else if(c < h){
				/* Wrap-over case */
				//if(h < (t + KTAU_TRACE_LAST)){
				//AN_20060201_TRC
				if(h < (t + lc_last_no)){
					/* Not last entry */
					current->ktau->cur_trace++;
					if(l == h){
						/* First we have to keep track of the period of time
						 * that the trace event are lost 
						 */
						if(current->ktau->first_lost.tsc == 0 &&
						   current->ktau->last_lost.tsc == 0){
							current->ktau->first_lost.tsc = current->ktau->last_dump->tsc;
							current->ktau->last_lost.tsc  = current->ktau->last_dump->tsc;
						}else {
							current->ktau->last_lost.tsc  = current->ktau->last_dump->tsc;
						}
						current->ktau->last_dump++;
						//printk("Warning! KTAU: trace_buf_write: Losing trace info.\n");
					}
					current->ktau->head_trace++;
				}else{
					/* Head is at the last entry, wrap over */
					if(l == h){
						/* First we have to keep track of the period of time
						 * that the trace event are lost 
						 */
						if(current->ktau->first_lost.tsc == 0 &&
						   current->ktau->last_lost.tsc == 0){
							current->ktau->first_lost.tsc = current->ktau->last_dump->tsc;
							current->ktau->last_lost.tsc  = current->ktau->last_dump->tsc;
						}else {
							current->ktau->last_lost.tsc  = current->ktau->last_dump->tsc;
						}
						current->ktau->last_dump = t;
						//printk("Warning! KTAU: trace_buf_write: Losing trace info.\n");
					}
					current->ktau->head_trace 	= t;
					current->ktau->cur_trace++;  
				}
			}
		}
	}
	return(0);
}

void* trace_buf_destroy(ktau_prof* prof) {
	void* tmp_ptr = NULL;

	prof->head_trace = NULL;
	prof->cur_trace  = NULL;
	prof->last_dump  = NULL;
	tmp_ptr 	      = (void*) prof->trbuf;
	prof->trbuf      = NULL;

	return(tmp_ptr);
}

/*--------------------------- OUT API ---------------------------------*/
/*
 * Note: We dump from the head to current
 */
static int dump_trace(ktau_prof *ktau,ktau_trace *out){
	int outCount = 0;
	ktau_trace *c = ktau->cur_trace;	 /* Current Pointer */
	ktau_trace *t = ktau->trbuf;		 /* Trace start Pointer */
	//ktau_trace *d = ktau->head_trace;	 /* Dumping Pointer */
	ktau_trace *d = ktau->last_dump;	 /* Dumping Pointer */

	//AN_20060201_TRC
	unsigned long lc_last_no = ktau->trace_last_no;

#if 0	
	printk("dump_trace: DUMPING TRACE\n");
	printk(" cur_trace 	= %x\n",ktau->cur_trace);
	printk(" head_trace 	= %x\n",ktau->head_trace);
	printk(" trbuf		= %x\n",ktau->trbuf);
	printk(" cur_trace->tsc = %llu\n",ktau->cur_trace->tsc);
	printk(" cur_trace->addr = %u\n",ktau->cur_trace->addr);
	printk(" cur_trace->type = %u\n",ktau->cur_trace->type);
	printk(" head_trace->tsc = %llu\n",ktau->head_trace->tsc);
	printk(" head_trace->addr = %u\n",ktau->head_trace->addr);
	printk(" head_trace->type = %u\n",ktau->head_trace->type);
	printk(" trbuf_trace->tsc = %llu\n",ktau->trbuf->tsc);
	printk(" trbuf->addr = %u\n",ktau->trbuf->addr);
	printk(" trbuf->type = %u\n",ktau->trbuf->type);
#endif

	if(!ktau->trbuf) {
		return -1;
	}

	/* Print out the information about the lost traces */
	if(ktau->first_lost.tsc != 0){
		/* First Lost */
		(out+outCount)->tsc = ktau->first_lost.tsc;	
		(out+outCount)->addr = ktau->first_lost.addr;	
		(out+outCount)->type = ktau->first_lost.type;
		outCount++;
		/* Last Lost */	
		(out+outCount)->tsc = ktau->last_lost.tsc;	
		(out+outCount)->addr = ktau->last_lost.addr;	
		(out+outCount)->type = ktau->last_lost.type;
		outCount++;
		ktau->first_lost.tsc = 0;	
		ktau->last_lost.tsc = 0;	
	}
	
	while(d != c){
		/* Dump current entry*/
		(out+outCount)->tsc 	= d->tsc;
		(out+outCount)->addr 	= d->addr;
		(out+outCount)->type	= d->type;
	//printk("tsc = %llu, ",d->tsc);
	//printk("addr = %u, ", d->addr);
	//printk("type = %u\n", d->type);
		outCount++;
			
		/* Increment d*/
		//if(d < (t + KTAU_TRACE_LAST)) 
		//AN_20060201_TRC
		if(d < (t + lc_last_no)) 
			d++;
		else 
			d = t; 
	}
	//printk("------ outCount = %d\n",outCount);
	
	/* Update last_dump pointer */
	ktau->last_dump = d;
	return(outCount);
}

/*
 * INPUT:
 * 	- ktau_prof *ktau	: a pointer to ktau_prof struct from task_struct
 * 	- ktau_trace *out	: a pointer to a buffer that will contain the trace
 */
int dump_trace_self(struct task_struct *tsk, ktau_prof *ktau, ktau_output *out)
{
	unsigned long flags = 0;

	//AN_20060201_TRC
	unsigned long lc_last_no = ktau->trace_last_no;

	//if(!(out->trace_lst = (ktau_trace*)vmalloc(sizeof(ktau_trace)*KTAU_TRACE_MAX+2)))
	//AN_20060201_TRC
	if(!(out->trace_lst = (ktau_trace*)vmalloc(sizeof(ktau_trace)*(lc_last_no + 2))))
	//if(!(out->trace_lst = (ktau_trace*)kmalloc(sizeof(ktau_trace)*KTAU_TRACE_MAX,
	//		GFP_KERNEL)))
	{
		err("dump_trace_self: Can't allocate memory for out.");
		return(-1);
	}
	
	/* Spin_lock */
        ktau_spin_lock(&tsk->ktau_lock,flags);
	out->size = dump_trace(ktau,out->trace_lst);
        ktau_spin_unlock(&tsk->ktau_lock,flags);
	return(0);
}

//#define for_each_process(p) for_each_task(p)
static int _dump_trace_all(ktau_output **arr)
{
        struct task_struct *cur_tsk;
        int count = 0;
        int cur_count= 0;

        down_write(&ktau_global_sem);

        read_lock(&tasklist_lock);
        
	for_each_process(cur_tsk) count++;

        *arr = kmalloc(sizeof(ktau_output) * count, GFP_ATOMIC);
	//why is this GFP_ATOMIC? might_sleep? cant be since we take a semaphore here ... 
	//tODO change to GFP_KERNEL

        for_each_process(cur_tsk){
                (*arr+cur_count)->pid = cur_tsk->pid;
                if(dump_trace_self(cur_tsk, cur_tsk->ktau, (*arr+cur_count)))
                        break;
                cur_count++;
        }
        read_unlock(&tasklist_lock);
        up_write(&ktau_global_sem);

        return cur_count;

}		              

int dump_trace_many(pid_t *pid_lst,
                unsigned int size,
                ktau_output **arr)
{
        int ret_count = 0;
        int cur_count = 0;
        struct task_struct *cur_tsk;

        /* Check if passing ALL */
        if(!size){
                if(arr) return _dump_trace_all(arr);
                else    return(-1);             /* Error arr is NULL */
        }else
                *arr = kmalloc(sizeof(ktau_output)*size,GFP_KERNEL);

        down_write(&ktau_global_sem);
        read_lock(&tasklist_lock);

        /* 
         * Searching through the task list for the specified process.
         * This should be optimized!!!!! Required the pid_lst to be 
         * sorted.
         */
#if 0
        for_each_process(cur_tsk){
                if(cur_tsk->pid == pid_lst[cur_count]){
                        /* The specified PID is found */
                        (*arr+cur_count)->pid = cur_tsk->pid;
                        if(dump_trace_self(cur_tsk->ktau, (*arr+cur_count)))
                                break;
                        cur_count++;
                        ret_count++;
                }else if(cur_tsk->pid > pid_lst[cur_count]){
                        /* The specified PID is not found */
                        cur_count++;
                }

                /* Check if we have filled upto the size allocated*/
                if(ret_count == size)
                        break;
        }

        read_unlock(&tasklist_lock);
        up_write(&ktau_global_sem);
#endif

	for_each_process(cur_tsk){
		do{
			if(cur_tsk->pid == pid_lst[cur_count]){
				/* The specified PID is found */
				
				/* Read the pid from the process task_struct */	
				(*arr+ret_count)->pid = cur_tsk->pid;
			
				/* Do the same thing as dump_hash_self */
				if(dump_trace_self(cur_tsk, cur_tsk->ktau, (*arr+ret_count)))
					break;

				/* Move to the next pid in the list */
				cur_count++;
				ret_count++;
			
			}else if(cur_tsk->pid > pid_lst[cur_count]){
				/* The specified PID is not found */
				/* Move to the next pid in the list */
				err("dump_trace_many : WARNING! PID %d not found.\n",
						pid_lst[cur_count]);	
				cur_count++;
			}
			/* Must check for array out-of-bound 
			 * before continue */
			if(cur_count >=  size || ret_count == size) 
				goto done;

		}while(cur_tsk->pid >= pid_lst[cur_count]);

	}
done:	
	read_unlock(&tasklist_lock);
	up_write(&ktau_global_sem);

        return ret_count;
}

/*
 * This function return the size of buffer required to dump trace data.
 * In case of "all", we also update the num_pid to the current number 
 * of processes.
 */
/*
int get_trace_size(int* num_pid){
        struct task_struct *cur_tsk;
	unsigned long max_count_sum = 0;

	if(*num_pid == 0){
		read_lock(&tasklist_lock);
		for_each_process(cur_tsk){
			if(cur_tsk->ktau) {
				*num_pid = (*num_pid) + 1;
				//AN_20060201_TRC
				max_count_sum += (cur_tsk->ktau->trace_max_no + 2);
			}
		}
		read_unlock(&tasklist_lock);
	} else {
		//AN_20060201_TRC
		max_count_sum = (cur_tsk->ktau->trace_max_no + 2);
	}	
	//return((*num_pid) * sizeof(ktau_trace) * (KTAU_TRACE_MAX+2)); 
	//AN_20060201_TRC
	return(sizeof(ktau_trace) * (max_count_sum)); 
}
*/

//AN_20060201_TRC
/*
 * INPUT:
 * 	- ktau_prof *ktau	: a pointer to ktau_prof struct from task_struct
 * 	- ktau_trace *out	: a pointer to a int that will contain the size
 */
int get_trace_size_self(struct task_struct *tsk, ktau_prof *ktau, int *psize)
{
	unsigned long flags = 0;

	/* Spin_lock */
        ktau_spin_lock(&tsk->ktau_lock,flags);
	*psize = (ktau->trace_max_no * sizeof(ktau_trace));
        ktau_spin_unlock(&tsk->ktau_lock,flags);
	return(0);
}


int get_trace_size_all(int *psize)
{
        struct task_struct *cur_tsk;
        int count= 0, this_size = 0;

	if(!psize) return -1;

	*psize = 0;

        down_write(&ktau_global_sem);

        read_lock(&tasklist_lock);
        
        for_each_process(cur_tsk){
		if(cur_tsk->ktau) {
			if(!get_trace_size_self(cur_tsk, cur_tsk->ktau, &this_size)) {
				*psize += this_size;
				count++;
			}
		}
        }
        read_unlock(&tasklist_lock);
        up_write(&ktau_global_sem);

        return count;

}		              


int get_trace_size_many(pid_t *pid_lst,
                unsigned int no_pids,
                int *psize)
{
        int ret_count = 0, this_size = 0;
        struct task_struct *cur_tsk;

	//init the return location
	*psize = 0;

        /* Check if passing ALL */
        if(!no_pids) {
                if(psize) {
			return get_trace_size_all(psize);
		} else {
			return(-1);             /* Error arr is NULL */
		}
	}

        down_write(&ktau_global_sem);
        read_lock(&tasklist_lock);

	while(no_pids) {
		cur_tsk = find_task_by_pid(pid_lst[no_pids-1]);

		if(!cur_tsk) {
			no_pids--;
			continue;
		}

		if(cur_tsk->ktau) {
			if(!get_trace_size_self(cur_tsk, cur_tsk->ktau, &this_size)) {
				*psize += this_size;
				ret_count++;
			}
		}

		no_pids--;
	}

	read_unlock(&tasklist_lock);
	up_write(&ktau_global_sem);

        return ret_count;
}

int trace_buf_resize_self(struct task_struct *tsk, ktau_prof* ktau, int newsize) {
	unsigned long flags = 0;
	int ret_val = 0;
	ktau_trace* oldmem = NULL;
	ktau_trace* newmem = NULL;

	//All alloc & de-alloc of v-mem should be done outside of the lock
	//	1. this is better as lock is held for less
	//	2. if we dont do this - then the lock will be held when an
	//	IPI is sent - interrupts may also be disabled. - not good
	//	leads to smp-badness
	
	//alloc new-mem
	newmem = __trace_buf_alloc(newsize);
	if(!newmem) {
		err("__trace_buf_alloc of Entries:%d , Size:%lu Failed.\n", newsize, newsize*sizeof(ktau_trace));
		return -1;
	}

	/* Spin_lock */
        ktau_spin_lock(&tsk->ktau_lock,flags);
	
	//remember old buffer and set it to NULL (free after releasing lock)
	oldmem = ktau->trbuf;
	ktau->trbuf = NULL;

	ret_val = __trace_buf_init(ktau, newsize, newmem);

        ktau_spin_unlock(&tsk->ktau_lock,flags);

	vfree(oldmem);

	return(ret_val);
}

int trace_buf_resize_all(int newsize)
{
        struct task_struct *cur_tsk;
        int count= 0;

        down_write(&ktau_global_sem);

        read_lock(&tasklist_lock);
        
        for_each_process(cur_tsk){
		if(cur_tsk->ktau) {
			if(!trace_buf_resize_self(cur_tsk, cur_tsk->ktau, newsize)) {
				count++;
			}
		}
        }
        read_unlock(&tasklist_lock);
        up_write(&ktau_global_sem);

        return count;

}		              

int trace_buf_resize_many(pid_t *pid_lst,
                unsigned int no_pids,
                int newsize)
{
        int ret_count = 0;
        struct task_struct *cur_tsk;

        /* Check if passing ALL */
        if(!no_pids) {
			return trace_buf_resize_all(newsize);
	}

        down_write(&ktau_global_sem);

	while(no_pids) {

		read_lock(&tasklist_lock);

		cur_tsk = find_task_by_pid(pid_lst[no_pids-1]);

		read_unlock(&tasklist_lock);

		if(!cur_tsk) {
			err("trace_buf_resize_many: Cannot find Pid:%d\n", pid_lst[no_pids-1]);
			no_pids--;
			continue;
		}

		if(cur_tsk->ktau) {
			if(!trace_buf_resize_self(cur_tsk, cur_tsk->ktau, newsize)) {
				ret_count++;
			}
		}

		no_pids--;
	}

	up_write(&ktau_global_sem);

        return ret_count;
}

#endif /*CONFIG_KTAU_TRACE*/

