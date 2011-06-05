/*************************************************************
 * File 	: kernel/ktau/ktau_hash.c
 * Version 	: $Id: ktau_hash.c,v 1.11.2.14 2008/11/19 05:26:50 anataraj Exp $
 *
 ************************************************************/

#include <linux/kernel.h>
//#include <linux/config.h> //breaks 2.6.19+
#include <linux/autoconf.h>
#include <linux/init.h>
#include <linux/linkage.h>      /* system call stuff */
#include <linux/sched.h>        /* task_struct def. */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/interrupt.h>
#include <linux/spinlock.h>     /* spinlock */
#include <linux/rwsem.h>        /* R/W semaphore */
//#include <linux/hardirq.h>	/* This might be needed for 2.6 */

#include <asm/current.h>	/* get_current */
#include <asm/uaccess.h>

#define TAU_NAME "ktau_hash"
#define PFX TAU_NAME

//#define TAU_DEBUG		/* to turn on debug messages */
#include <linux/ktau/ktau_print.h>
#include <linux/ktau/ktau_hash.h>
#include <linux/ktau/ktau_api.h>
#include <linux/ktau/ktau_proc.h>
#include <linux/ktau/ktau_mmap.h>
#include <linux/ktau/ktau_bootopt.h>

#include "ktau_linux-2.4.c"

extern rwlock_t tasklist_lock;

/*----------------------- KTAU GLOBAL SEMAPHORE -----------------------*/
struct rw_semaphore ktau_global_sem;
void ktau_global_semaphore_init(void){
	init_rwsem(&ktau_global_sem);
}

/*--------------------------- IN API ---------------------------------*/
/* only to be called on tasks which are about to be removed - i.e. the "fromtsk" should be dead */
void transfer_task_profile(struct task_struct *fromtask, struct task_struct *totask) {
        //from is usually a child
        //to is usually the parent
        unsigned long kflags = 0;
        int nocopied = 0, true_frmcount = 0, i = 0;
        h_ent *from_h_ptr, *to_h_ptr;
        ktau_prof *frombuf, *tobuf;

        if((!fromtask) || (!totask)) {
                err("transfer_task_profile: null tasks: from:%p  to:%p\n", fromtask, totask);
                return;
        }

        frombuf = fromtask->ktau;
        tobuf = totask->ktau;
        true_frmcount = frombuf->ent_count;

        //is it sufficient to lock only "to" ,as "from" tsk is dead?
        ktau_spin_lock(&totask->ktau_lock,kflags);

        /* Searching through hash table */
        for(i=0;i<KTAU_HASH_SIZE;i++){
                from_h_ptr = (h_ent*)(frombuf->ktau_hash[i]);
                if(from_h_ptr){

                        to_h_ptr = (h_ent*)(tobuf->ktau_hash[i]);
			if(!to_h_ptr) {
				//AN_20060419_OPTIMZ
				//current->ktau->ktau_hash[index] = kmalloc(sizeof(h_ent),GFP_ATOMIC);
				if(!tobuf->hash_mem) {
					goto ktau_unlock;
				}
				tobuf->ktau_hash[i] = (tobuf->hash_mem+i);

                                *(tobuf->ktau_hash[i]) = *from_h_ptr;

                                //init other state inside h_ptr
                                tobuf->ktau_hash[i]->nestingLevel = 0;
                                tobuf->ktau_hash[i]->data.timer.start = 0;

                                tobuf->ent_count++;
			} else {               
/*
                        if(!to_h_ptr) {
                                //then just hand over the memory
                                tobuf->ktau_hash[i] = from_h_ptr;
                                frombuf->ktau_hash[i] = NULL;
                                frombuf->ent_count--;
                                tobuf->ent_count++;

                                //init other state inside h_ptr
                                tobuf->ktau_hash[i]->nestingLevel = 0;
                                tobuf->ktau_hash[i]->data.timer.start = 0;
                        } else {
*/
                                //otherwise sum it
                                to_h_ptr->data.timer.count += from_h_ptr->data.timer.count;
                                to_h_ptr->data.timer.incl += from_h_ptr->data.timer.incl;
                                to_h_ptr->data.timer.excl += from_h_ptr->data.timer.excl;
                        }

                        nocopied++;

                        /* Check if all data are coppied over. */
                        if(nocopied >= true_frmcount)
                                break;
                }
        }
ktau_unlock:
        ktau_spin_unlock(&totask->ktau_lock,kflags);
}

/*
 * This function initialize ktau_prof structure in task_struct.
 * 	1. Allocate the hash-table which contains pointer 
 * 	   to profile information (h_ent).
 * 	2. Initialize spinlock.
 * 	3. Create the corresponding entry in /proc/<pid>/ktau.
 */
void create_task_profile(struct task_struct *tsk){

	//AN_20060419_OPTIMZ
	/*
	if(!(tsk->ktau = kmalloc(sizeof(ktau_prof),GFP_KERNEL))){
                err("Can't allocate memory for ktau_prof.");
	}else{
		memset(tsk->ktau->ktau_hash,0,KTAU_HASH_SIZE * sizeof(h_ent*));

		tsk->ktau->hash_mem = 0;

		if(!(tsk->ktau->hash_mem = kmalloc(sizeof(h_ent) * KTAU_HASH_SIZE, GFP_KERNEL))) {
			err("Can't allocate memory for ktau_prof->hash_mem: size:%d\n", sizeof(h_ent) * KTAU_HASH_SIZE);
		}
	}
	*/
	//AN_20060419_OPTIMZ_2
	//calculate total space required for the profile and allocate it
	char* tot_space = NULL;
	int tot_num = 0;

#ifndef CONFIG_KTAU_TRACE /* not defined TRACE -then do profile */
	tot_num = (/*for tsk->ktau*/ sizeof(ktau_prof)) +  (/*for h_ents*/ sizeof(h_ent) * KTAU_HASH_SIZE); 
	if(!(tot_space = kmalloc(tot_num, GFP_KERNEL))) {
                err("Can't allocate total memory for ktau_prof.");
		tsk->ktau = NULL;
		return;
	}
	//assign to the different entities from the tot_space
	tsk->ktau = (ktau_prof*) tot_space;
	tot_space += sizeof(ktau_prof);
	memset(tsk->ktau->ktau_hash,0,KTAU_HASH_SIZE * sizeof(h_ent*));
	tsk->ktau->hash_mem = (h_ent*) tot_space;
	//done assignment of memory -----
	
	/* Initialize entry counter */
	tsk->ktau->ent_count 	= 0;
	memset(&tsk->ktau->accum,0,
			sizeof(unsigned long long)*KTAU_NESTED_LEVEL+1);
	//FIX MERGE
	memset(&(tsk->ktau->merge), 0, sizeof(ktau_user));

	//AN_20051102_REENT
	memset(&tsk->ktau->reent,0, sizeof(unsigned long long)*(KTAU_NESTED_LEVEL+1));
	tsk->ktau->stackDepth = 0;
	tsk->ktau->last_sysc = -(0xABBB);

	//AN_20070301_SHCONT
	memset(&tsk->ktau->shconts, 0, sizeof(ktau_shcont)*KTAU_MAX_SHCONTS);
	tsk->ktau->no_shconts = 0;
	//AN_20070309_SHCTR
	INIT_LIST_HEAD(&(tsk->ktau->shctr_list));
	INIT_LIST_HEAD(&(tsk->ktau->pend_evs_list));
	tsk->ktau->no_shctrs = tsk->ktau->no_pend = 0;
	memset(&(tsk->ktau->htrigger_list), 0, sizeof(struct list_head*)*KTAU_HASH_SIZE);
#else /*this means Trace is defined - therefore dont do profile*/

	tot_num = (/*for tsk->ktau*/ sizeof(ktau_prof));
	if(!(tot_space = kmalloc(tot_num, GFP_KERNEL))) {
                err("Can't allocate total memory for ktau_prof(size:%d).", sizeof(ktau_prof));
		tsk->ktau = NULL;
		return;
	}
	//assign to the different entities from the tot_space
	tsk->ktau = (ktau_prof*) tot_space;
	//memset(tsk->ktau->ktau_hash,0,KTAU_HASH_SIZE * sizeof(h_ent*));
	tsk->ktau->hash_mem = NULL;
	//done assignment of memory -----

	//must do this for both trace and profile
	tsk->ktau->last_sysc = -(0xABBB);

#ifdef CONFIG_KTAU_BOOTOPT
	if(ktau_verify_bootopt(KTAU_TRACE_MSK))
#endif /* CONFIG_KTAU_BOOTOPT*/
	{
		tsk->ktau->trbuf	= NULL;
		tsk->ktau->cur_trace	= NULL;
		tsk->ktau->head_trace	= NULL;
		trace_buf_init(tsk);	
	}
#endif /*CONFIG_KTAU_TRACE*/

	tsk->ktau->disable = 0;

	/* Initialize hash table lock */
	spin_lock_init(&tsk->ktau_lock);

	/* Point internal hash-lock to ktau_lock in task_struct */
	/* - so lots of code doesn't break. Should change rest  */
	/* of code that breaks... */
	tsk->ktau->hash_lock = &(tsk->ktau_lock);

}

/* Assumes that lock is held *if needed*
 * by caller. So can either be called
 * right during task exit, or by events
 * workqueue */
void free_ktau_profile(pid_t pid, ktau_prof* prof, char** temp_trace) {
	
	prof->hash_mem = NULL;
#ifndef CONFIG_KTAU_TRACE
	if(unlikely(prof->merge.uptr != 0)) {
	       //TODO: Must get rid of the mmaped 'merge' pages	
	       //the process died before it cleaned up properly.
	       err("free_ktau_profile: WARN: merge.uptr != NULL. pid:%d\n", pid);	
	}
	if(unlikely((prof->no_pend) || (prof->no_shctrs) || (prof->no_shconts))) {
		//TODO: Must get rid of "shared" - triggers, counters and containers
		//process died without cleanup...
	       err("free_ktau_profile: WARN: no_pend:%d no_shctrs:%d no_shconts:%d. pid:%d\n", prof->no_pend, prof->no_shctrs, prof->no_shconts, pid);	
	}
#endif
#ifdef CONFIG_KTAU_TRACE
#ifdef CONFIG_KTAU_BOOTOPT
	if(ktau_verify_bootopt(KTAU_TRACE_MSK))
#endif /* CONFIG_KTAU_BOOTOPT*/
	{
		/* Free ktau_prof */
		*temp_trace = trace_buf_destroy(prof);
	}
#endif /*CONFIG_KTAU_TRACE*/

	kfree(prof);

#ifdef CONFIG_KTAU_TRACE
	trace_buf_free(temp_trace);
#endif /*CONFIG_KTAU_TRACE*/

}



void remove_ktau_profile(spinlock_t* lock, pid_t pid, ktau_prof* prof) {
	unsigned long kflags = 0;
#ifdef CONFIG_KTAU_TRACE
	void *temp_trace = NULL;

#endif /*CONFIG_KTAU_TRACE*/

	/* DONT DO this parent thing - typically unnecessary - make it an option
	if((tsk->parent)  && (tsk->parent->ktau)) {
		transfer_task_profile(tsk, tsk->parent);
	}
	*/

	if(lock) ktau_spin_lock(lock,kflags);

	prof->hash_mem = NULL;

#ifndef CONFIG_KTAU_TRACE
	if(unlikely(prof->merge.uptr != 0)) {
	       //TODO: Must get rid of the mmaped 'merge' pages	
	       //the process died before it cleaned up properly.
	       err("remove_task_profile: WARN: merge.uptr != NULL. pid:%d\n", pid);	
	}
#endif

#ifdef CONFIG_KTAU_TRACE
#ifdef CONFIG_KTAU_BOOTOPT
	if(ktau_verify_bootopt(KTAU_TRACE_MSK))
#endif /* CONFIG_KTAU_BOOTOPT*/
	{
		/* Free ktau_prof */
		temp_trace = trace_buf_destroy(prof);
	}
#endif /*CONFIG_KTAU_TRACE*/

	if(lock) ktau_spin_unlock(lock,kflags);

#ifdef CONFIG_KTAU_TRACE
	trace_buf_free(temp_trace);
#endif /*CONFIG_KTAU_TRACE*/

#ifndef CONFIG_KTAU_TRACE
	//do GC on shared stuff
	if(prof->no_shctrs) {
		if(ktau_shctr_gc(pid, prof)) {
			err("remove_task_profile: warn ktau_shctr_gc returned err.");
		}
	}
	if(prof->no_shconts) {
		if(ktau_shcont_gc(pid, prof)) {
			err("remove_task_profile: warn ktau_shcont_gc returned err.");
		}
	}
#endif /* CONFIG_KTAU_TRACE */
	kfree(prof);
}

/*
 * This function handles clean up for ktau_prof structure
 * in task_struct.
 * 	1. Freeing all the allocated profiles.
 */
void remove_task_profile(struct task_struct* task) {
	remove_ktau_profile(&(task->ktau_lock), task->pid, task->ktau);
	spin_lock_init(&task->ktau_lock);
	task->ktau = NULL;
}

/* data-struct to hold work
 */
typedef struct _ktau_wq_data {
	struct work_struct work;	//keep it here so we only need 1 kmalloc
	pid_t pid;			//pid of process which this work is associated to
	ktau_prof* prof;		//the ktau_prof ptr
} ktau_wq_data;

/* the work-queue version */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20) )
void remove_task_profile_wq(void* wdata) {
	remove_ktau_profile(NULL, ((ktau_wq_data*)wdata)->pid, ((ktau_wq_data*)wdata)->prof);
	kfree(wdata);
}

/* schedule the work of removing task profile... */
void remove_task_profile_sched(struct task_struct* task) {
	ktau_wq_data* wdata = NULL;

	wdata = kmalloc(sizeof(ktau_wq_data), GFP_ATOMIC); //we use this method only if we are in non-sleeping area
	if(!wdata) {
		err("remove_task_profile_sched: Couldnt kmalloc GFP_ATOMIC (wdata) of size:%d. Work not queued.", sizeof(ktau_wq_data));
		return;
	}
	
	wdata->pid = task->pid;
	wdata->prof = task->ktau;

	INIT_WORK(&(wdata->work), remove_task_profile_wq, wdata);

	schedule_work(&(wdata->work));

	task->ktau = NULL;
}
#else /* ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20) ) */
void remove_task_profile_wq(struct work_struct *thework) {
	ktau_wq_data *wdata = container_of(thework, ktau_wq_data, work);
	remove_ktau_profile(NULL, wdata->pid, wdata->prof);
	kfree(wdata);
}

/* schedule the work of removing task profile... */
void remove_task_profile_sched(struct task_struct* task) {
	ktau_wq_data* wdata = NULL;

	wdata = kmalloc(sizeof(ktau_wq_data), GFP_ATOMIC); //we use this method only if we are in non-sleeping area
	if(!wdata) {
		err("remove_task_profile_sched: Couldnt kmalloc GFP_ATOMIC (wdata) of size:%d. Work not queued.", sizeof(ktau_wq_data));
		return;
	}
	
	wdata->pid = task->pid;
	wdata->prof = task->ktau;

	INIT_WORK(&(wdata->work), remove_task_profile_wq);

	schedule_work(&(wdata->work));

	task->ktau = NULL;
}
#endif /* ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20) ) */

/*--------------------------- OUT API ---------------------------------*/
/*
 * This function handles dumping the profile hash-table
 */
int dump_hash_self(struct task_struct* tsk, ktau_prof *buf, 
		ktau_output *out, 
		unsigned long long tol)
{
        int i;
	unsigned long kflags = 0;
	h_ent* h_ptr; 
	o_ent* o_ptr; 

	if(out == NULL)
		return -1;

	out->size = 0;
	out->ent_lst = kmalloc(sizeof(o_ent) * buf->ent_count , GFP_KERNEL);

	ktau_spin_lock(&tsk->ktau_lock,kflags);

	/* Searching through hash table */
	for(i=0;i<KTAU_HASH_SIZE;i++){
		h_ptr = (h_ent*)(buf->ktau_hash[i]);
		if(h_ptr){
						
			/* Copy data over */
			o_ptr = (out->ent_lst + out->size);
			o_ptr->index = i;
			memcpy(&(o_ptr->entry),h_ptr, sizeof(h_ent));
			
			/*
			printk(KERN_INFO "dump_hash_self: Entry %4d: addr %x, count %4u, start %4llu, incl %4llu, excl %4llu\n",
				i,
				o_ptr->entry.addr, 
				o_ptr->entry.data.timer.count,
				o_ptr->entry.data.timer.start,
				o_ptr->entry.data.timer.incl,
				o_ptr->entry.data.timer.excl);
			*/		
			out->size++;

			/* Check if all data are coppied over. */
			if(out->size >= buf->ent_count) 
				break;
		}
        }

	ktau_spin_unlock(&tsk->ktau_lock,kflags);
	return 0;
}

/* 
 * This function returns number of entries for 
 * "self".
 */ 
int get_profile_size_self(ktau_prof *buf)
{
	int count	= 0;
	int i		= 0;

	for(i=0;i<KTAU_HASH_SIZE;i++){
		if(buf->ktau_hash[i])
			count++;
	}
	
	return(count);
}

/* 
 * This function returns number of entries for 
 * "many".
 */ 
int get_profile_size_many(pid_t *pid_lst, int* num_pid)
{
	struct task_struct *cur_tsk, *g;
	int count	= 0;
	int i		= 0;
	int cur_count   = 0;
	
	/* Case for all */
	if(*num_pid == 0){
		read_lock(&tasklist_lock);
		//for_each_process(cur_tsk){
		do_each_thread(g, cur_tsk){
			*num_pid = (*num_pid) + 1;
			for(i=0;i<KTAU_HASH_SIZE;i++){
				if(cur_tsk->ktau->ktau_hash[i])
					count++;
			}
		} while_each_thread(g, cur_tsk);
		read_unlock(&tasklist_lock);	
	}
	/* Case for many pids */	
	else{
		read_lock(&tasklist_lock);
#if 0
		for_each_process(cur_tsk){
			if(cur_tsk->pid == pid_lst[cur_count]){
				/* The specified PID is found */
				for(i=0;i<KTAU_HASH_SIZE;i++){
					if(cur_tsk->ktau->ktau_hash[i])
						count++;
				}
				cur_count++;
			}else if(cur_tsk->pid > pid_lst[cur_count]){
				/* The specified PID is not found */
				cur_count++;
			}
		}
#endif
		//for_each_process(cur_tsk){
		do_each_thread(g, cur_tsk){
			do{
				if(cur_tsk->pid == pid_lst[cur_count]){
					/* The specified PID is found */
					
					for(i=0;i<KTAU_HASH_SIZE;i++){
						if(cur_tsk->ktau->ktau_hash[i])
							count++;
					}

					/* Move to the next pid in the list */
					cur_count++;
				
				}else if(cur_tsk->pid > pid_lst[cur_count]){
					/* The specified PID is not found */
					/* Move to the next pid in the list */
					err("get_profile_size_many : WARNING! PID %d not found.\n",
							pid_lst[cur_count]);	
					cur_count++;
				}
				/* Must check for array out-of-bound 
				 * before continue */
				if(cur_count >=  *num_pid) 
					goto done;

			}while(cur_tsk->pid >= pid_lst[cur_count]);
		} while_each_thread(g, cur_tsk);
done:
		read_unlock(&tasklist_lock);	
	}
	return(count);
}
/*
 * This function handles purging the profile hash-table
 */
int purge_hash_self(ktau_prof *buf, 
		unsigned long long tol)
{
#if 0
        int i;
	unsigned long flags = 0;
	h_ent* h_ptr;

	ktau_spin_lock(&buf->hash_lock,flags);
		for(i=0;i<KTAU_HASH_SIZE;i++){
			h_ptr = (h_ent*)(buf->ktau_hash[i]);
			if(h_ptr){
				h_ptr->data.timer.count = 0;	
				h_ptr->data.timer.start = 0;	
				h_ptr->data.timer.incl = 0;
				h_ptr->data.timer.excl = 0;
			}
		}
	ktau_spin_unlock(&buf->hash_lock,flags);
#endif
	return 0;
}
/*-------------------------------------------------------------------*/
#if 0 
/* Replaced by dump_hash_many and purge_hash_many */
/*
 * Wrapper of dump_hash_self w/ locking 
 */
int dump_hash_other(ktau_prof *buf, 
		ktau_output *out, 
		unsigned long long tol)
{
	int ret;
	/*
	 * First: Acquire the global semaphore and tasklist_lock
	 */
	down_write(&ktau_global_sem);
	read_lock(&tasklist_lock);

	ret = dump_hash_self(buf,out,tol);

	/*Last : Release the global semaphore and tasklist_lock*/
	read_unlock(&tasklist_lock);
	up_write(&ktau_global_sem);
	return ret;
}

/*
 * Wrapper of purge_hash_self w/ locking
 */
int purge_hash_other(ktau_prof *buf, 
		unsigned long long tol)
{
	int ret;
	down_write(&ktau_global_sem);
	read_lock(&tasklist_lock);

	ret = purge_hash_self(buf,tol);

	/*Last : Release the global semaphore and tasklist_lock*/
	read_unlock(&tasklist_lock);
	up_write(&ktau_global_sem);
	return ret;
}
#endif 
/*-------------------------------------------------------------------*/
/*
 * This is wrapped by dump_hash_many in case we are dumping all.
 */

static int _dump_hash_all(ktau_output **arr,
		unsigned long long tol)
{
	struct task_struct *cur_tsk, *g;
	int count = 0;
	int cur_count= 0;

	down_write(&ktau_global_sem); //do we really need to take both glob_sem and tasklist_lock? becoz we only take read-lock of task-list?

	read_lock(&tasklist_lock);

	//get a better way to count - isnt there a kernel task count?
	//for_each_process(cur_tsk) count++;
	do_each_thread(g, cur_tsk) {
		count++;
	} while_each_thread(g, cur_tsk);

	*arr = kmalloc(sizeof(ktau_output) * count, GFP_ATOMIC); //TODO: Get rid of ATOMIC - unlock and relock	

	//for_each_process(cur_tsk){
	do_each_thread(g, cur_tsk){
		(*arr+cur_count)->pid = cur_tsk->pid;
		if(dump_hash_self(cur_tsk, cur_tsk->ktau, (*arr+cur_count), tol))
			goto done;
		cur_count++;
	} while_each_thread(g, cur_tsk);
done:
	read_unlock(&tasklist_lock);
	up_write(&ktau_global_sem);

	return cur_count;
}

/*
 * This is wrapped by purge_hash_all in case we are purging all. 
 */
#if 0
static int _purge_hash_all(unsigned long long tol)
{
	int cur_count = 0;
	struct task_struct *cur_tsk;

	down_write(&ktau_global_sem);
	read_lock(&tasklist_lock);
	for_each_process(cur_tsk){
		if(purge_hash_self(cur_tsk->ktau, tol))
			break;
		cur_count++;
	}

	read_unlock(&tasklist_lock);
	up_write(&ktau_global_sem);

	return cur_count;
}
#endif
/*-------------------------------------------------------------------*/
/*
 * dump_hash_many: Perform dump on other and all.
 */
int dump_hash_many(pid_t *pid_lst,
		unsigned int size, 
		ktau_output **arr, 
		unsigned long long tol)
{
	int ret_count = 0;
	int cur_count = 0;
	struct task_struct *cur_tsk, *g;

	/* Check if passing ALL */
	if(!size){
		if(arr) return _dump_hash_all(arr,tol);
		else    return(-1);		/* Error arr is NULL */
	}else
		*arr = kmalloc(sizeof(ktau_output)*size,GFP_KERNEL);
	
	down_write(&ktau_global_sem);
	read_lock(&tasklist_lock);

	/* 
	 * Searching through the task list for the specified process.
	 * This should be optimized!!!!! Required the pid_list to be 
	 * sorted.
	 */

	//for_each_process(cur_tsk){
	do_each_thread(g, cur_tsk){
		do{
			if(cur_tsk->pid == pid_lst[cur_count]){
				/* The specified PID is found */
				
				/* Read the pid from the process task_struct */	
				(*arr+ret_count)->pid = cur_tsk->pid;
			
				/* Do the same thing as dump_hash_self */
				if(dump_hash_self(cur_tsk, cur_tsk->ktau, (*arr+ret_count), tol))
					break;

				/* Move to the next pid in the list */
				cur_count++;
				ret_count++;
			
			}else if(cur_tsk->pid > pid_lst[cur_count]){
				/* The specified PID is not found */
				/* Move to the next pid in the list */
				err("dump_hash_many : WARNING! PID %d not found.\n",
						pid_lst[cur_count]);	
				cur_count++;
			}
			/* Must check for array out-of-bound 
			 * before continue */
			if(cur_count >=  size || ret_count == size) 
				goto done;

		}while(cur_tsk->pid >= pid_lst[cur_count]);

	}while_each_thread(g, cur_tsk);
done:	
	read_unlock(&tasklist_lock);
	up_write(&ktau_global_sem);
	return ret_count;
}
/*
 * purge_hash_many: Perform purge on other and all.
 */
int purge_hash_many(pid_t *pid_lst,
		unsigned int size, 
		unsigned long long tol)
{
	int ret_count = 0;
#if 0
	int cur_count = 0;
	struct task_struct *cur_tsk;

	/* Check if passing ALL */
	if(!size) 
		return _purge_hash_all(tol);


	down_write(&ktau_global_sem);
	read_lock(&tasklist_lock);

	/* 
	 * Searching through the task list for the specified process.
	 * This should be optimized!!!!! Required the pid_list to be 
	 * sorted.
	 */
	for_each_process(cur_tsk){
		if(cur_tsk->pid == pid_lst[cur_count]){
			/* The specified PID is found */
			if(purge_hash_self(cur_tsk->ktau,tol))
				break;
			cur_count++;
			ret_count++;
		}else if(cur_tsk->pid > pid_lst[cur_count]){
			/* The specified PID is not found */
			cur_count++;
		}

		/* Check if we have worked  upto the size specified*/
		if(ret_count == size)
			break;
	}

	read_unlock(&tasklist_lock);
	up_write(&ktau_global_sem);

#endif
	return ret_count;
}
/*--------------------------- CONTROL API ---------------------------------*/
/*  Merging changed & moved to ktau_merge.c / ktau_merge.h */

/* Event registration */

/* MUST be called with tsk->ktau_lock HELD.
 * On sucess returns 0. -ve on error.
 * Note: on return the tsk->ktau_lock will still
 * be HELD. But during the routine the lock may
 * be released and re-acquired - this is important!
 */
int ktau_reg_single_ev(struct task_struct* tsk, int index, ktau_trigger* new_ev, unsigned long* pirqflags) {
	struct list_head* tmp_head = NULL;
	int do_free = 0;
	unsigned long kflags = *pirqflags;
	//register
	if(tsk->ktau->htrigger_list[index] == NULL) {
		//release lock
		ktau_spin_unlock(&tsk->ktau_lock,kflags);
		
		//if(in_interrupt()) {
		if(in_atomic() || irqs_disabled()) {
			info("ktau_reg_single_ev: (in_atomic() || irqs_disabled()).");
			tmp_head = (struct list_head*)kmalloc(sizeof(struct list_head), GFP_ATOMIC);
		} else {
			info("ktau_reg_single_ev: NOT (in_atomic() || irqs_disabled())..");
			tmp_head = (struct list_head*)kmalloc(sizeof(struct list_head), GFP_KERNEL);
		}
		if(!tmp_head) {
			err("ktau_reg_single_ev: kmallloc failed for new struct list_head.");
			goto out_err;
		}
		INIT_LIST_HEAD(tmp_head);

		//acquire lock
		ktau_spin_lock(&tsk->ktau_lock,kflags);

		//now check again if(tsk->ktau->htrigger_list[index] == NULL)
		//why? after lock release an interrupt could have intervened 
		//and performed this same code. After we acqiore lock we may over-
		//-write work already done.
		if(tsk->ktau->htrigger_list[index] != NULL) {
			//remember to free the tmp_head and dont use it
			do_free = 1;
			info("ktau_reg_single_ev: do_free = 1.");
		} else {
			tsk->ktau->htrigger_list[index] = tmp_head;
		}
	} 
	list_add(&(new_ev->list), tsk->ktau->htrigger_list[index]);
	new_ev->pending = 0;//not pending anymore
	new_ev->hash_index = index; //so that we can dereg
	
	if(do_free) {
		//release, free and re-acquire lock
		info("ktau_reg_single_ev: freeing...");

		//release lock
		ktau_spin_unlock(&tsk->ktau_lock,kflags);
	
		kfree(tmp_head);	

		//acquire lock
		ktau_spin_lock(&tsk->ktau_lock,kflags);
	}
	//need to tell caller the state of irqsave/restore flags
	*pirqflags = kflags;
	return 0;
out_err:
	//acquire lock
	ktau_spin_lock(&tsk->ktau_lock,kflags);
	//need to tell caller the state of irqsave/restore flags
	*pirqflags = kflags;
	return -1;
}	

/* ktau_reg_ctrevents: find and register with events that ctr interested in	*
 * , use the default shared-ctr handler - for now. 	
 * Returns +ve number of events succesfully registered.
 * This no may not be equal to no_syms -- some maybe pending.
 * If its zero - then is it an error? not necessarily..
 * -ve is error. */
int ktau_reg_ctrevents(struct task_struct* tsk, ktau_sh_ctr* k_shctr, KTAU_EVHDR_CTR ev_handler) {
	//currently this is terribly inefficient 
	//reason: no good mapping from event -to- evId
	//because it is indexed, we can easily go from
	// evId to event, but not vice-versa, unless
	//we maintain another data-struct for that purpose
	//sigh - registration hopefully isnt frequently done!

	int sym_index = 0, index = 0, ret = 0;
	h_ent** hashp = tsk->ktau->ktau_hash;
	unsigned long* symp = k_shctr->desc.syms;
	ktau_trigger *new_ev = NULL;
	//struct list_head *htriggerlp = NULL, *tmp_head = NULL;
	unsigned long kflags = 0;

	info("ktau_reg_ctrevents: Enter: tsk->pid:%d k_shctr:%p ev_handler:%p", tsk->pid, k_shctr, ev_handler);

	//alloc all event triggers in one go - so easy to handle kmalloc error
	new_ev = (ktau_trigger*)kmalloc(sizeof(ktau_trigger)*k_shctr->desc.no_syms, GFP_KERNEL);	
	if(!new_ev) {
		err("ktau_reg_ctrevents: kmalloc of events [no_syms: %d] failed, tot_size:%d.", k_shctr->desc.no_syms, sizeof(ktau_trigger)*k_shctr->desc.no_syms);
		goto out_free;
	}

	//acquire lock
	ktau_spin_lock(&tsk->ktau_lock,kflags);

	//go thro the symbols of the ctr
	for(sym_index = 0; sym_index < k_shctr->desc.no_syms; sym_index++) {
		info("ktau_reg_ctrevents: sym loop: checking for:%lx .",symp[sym_index]);
		//init
		new_ev[sym_index].sym = symp[sym_index];
		new_ev[sym_index].index = sym_index;
		new_ev[sym_index].hash_index = -1; //dont know what yet
		new_ev[sym_index].handler = ev_handler;
		new_ev[sym_index].ctr = k_shctr;
		new_ev[sym_index].pending = 1; //before searching pending is true
		//go thro tsk->ktau->
		for(index = 0; index < KTAU_HASH_SIZE; index++) {
			/*if(hashp[index]) {
				AN_ERR("ktau_reg_ctrevents: hash loop: checking hash sym:%x .",hashp[index]->addr);
			}*/
			if((!hashp[index]) || (hashp[index]->addr != (unsigned int)(symp[sym_index]))) {
				continue;
			}
			info("ktau_reg_ctrevents: Found target hash-index [%d] for sym [%lx]", index, symp[sym_index]);
			//register
			if(ktau_reg_single_ev(tsk, index, &(new_ev[sym_index]), &kflags) < 0) {
			//if(ktau_reg_single_ev(tsk, index, &(new_ev[sym_index])) < 0) {
					err("ktau_reg_ctrevents: ktau_reg_single_ev failed.");
					goto out_dereg;
			}
			ret++;
			break;
		}

		if(new_ev[sym_index].pending) {
			tsk->ktau->no_pend++;
			list_add(&(new_ev[sym_index].list), &(tsk->ktau->pend_evs_list));
		}
	}
	
	//save new_evs, so that on de_reg we can free
	k_shctr->triggers = new_ev;

	//release lock
	ktau_spin_unlock(&tsk->ktau_lock,kflags);

	info("ktau_reg_ctrevents: Exit: tsk->pid:%d tot_no evs:%d  no_pending:%d", tsk->pid, k_shctr->desc.no_syms, (k_shctr->desc.no_syms - ret));
	return ret;

out_dereg:
	//should already be unlocked....- not after ktau_reg_single_ev ret error...
	//release lock
	ktau_spin_unlock(&tsk->ktau_lock,kflags);

	ktau_unreg_ctrevents(tsk, k_shctr);

out_free:
	kfree(new_ev);
	return -1;
}


/* ktau_unreg_ctrevents: find and unregister events 
 * -ve is error.  0 Success.*/
int ktau_unreg_ctrevents(struct task_struct* tsk, ktau_sh_ctr* k_shctr) {
	int evno = 0;
	unsigned long kflags = 0;
	void* tmp = NULL;

	if(!k_shctr->triggers) {
		err("ktau_unreg_ctrevents: no triggers... k_shctr->triggers:NULL.");
		return -1;
	}

	//acquire lock
	ktau_spin_lock(&tsk->ktau_lock,kflags);
	for(evno = 0; evno<k_shctr->desc.no_syms; evno++) {
		list_del(&(k_shctr->triggers[evno].list));
		if(k_shctr->triggers[evno].pending) {
			tsk->ktau->no_pend--;
		} else if( (tsk->ktau->htrigger_list[k_shctr->triggers[evno].hash_index]) && list_empty(tsk->ktau->htrigger_list[k_shctr->triggers[evno].hash_index]) ) {
			tmp = (void*)tsk->ktau->htrigger_list[k_shctr->triggers[evno].hash_index];
			tsk->ktau->htrigger_list[k_shctr->triggers[evno].hash_index] = NULL;
			//dont want to call kfree with lock....?
			ktau_spin_unlock(&tsk->ktau_lock,kflags);
			kfree(tmp);
			ktau_spin_lock(&tsk->ktau_lock,kflags);
		}
		k_shctr->triggers[evno].sym = 0xdeadbeef;
	}
	tmp = (void*)k_shctr->triggers;
	k_shctr->triggers = NULL;

	//release lock
	ktau_spin_unlock(&tsk->ktau_lock,kflags);
	kfree(tmp);
	return 0;
}
