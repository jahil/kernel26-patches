/******************************************************************************
 * File 	: kernel/ktau/ktau_merge.c
 * Version	: $Id: ktau_merge.c,v 1.2.2.1 2007/03/18 02:19:25 anataraj Exp $
 * ***************************************************************************/ 

/* to enable debug messages */
//#define TAU_DEBUG 

/* kernel headers */
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <asm/current.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/vmalloc.h>	/* for vmalloc */

/* generic ktau headers */
#define TAU_NAME "ktau_merge" 
#include <linux/ktau/ktau_print.h>
#include <linux/ktau/ktau_datatype.h>
#include <linux/ktau/ktau_api.h>

#include <linux/ktau/ktau_mmap.h>
#include <linux/ktau/ktau_merge.h>

#include <linux/ktau/ktau_misc.h>

#include <linux/time.h>
//fwd declaration for sys_ktau_gettimeofday
extern struct timezone sys_tz;

//2. set/unset user-state merging routine
int set_hash_mergestate(struct _ktau_prof *buf,
                ktau_state* pstate)
{
        unsigned long kflags = 0;
        int ret_value = 0;
        ktau_user lc_merge;

        //any request (SET or UNSET), if state was previously SET, then we must UNSET that before proceeding
        if(buf->merge.uptr != 0) {
                //take the lock, as weare going to change kernel-merge state
                ktau_spin_lock(buf->hash_lock,kflags);

                //copy the state locally
                lc_merge.uptr = buf->merge.uptr;
                lc_merge.orig_kaddr = buf->merge.orig_kaddr;
                lc_merge.pages = buf->merge.pages;
                lc_merge.nr_pages = buf->merge.nr_pages;

		//AN_20051216_FIXMERGE
		//lc_merge.lc_state = buf->merge.lc_state;


                //reset the kernel-state
                buf->merge.uptr = 0;
                buf->merge.orig_kaddr = 0;
                buf->merge.pages = NULL;
                buf->merge.nr_pages = 0;
                //reset lc_state - how?
		memset(&(buf->merge.lc_state), 0, sizeof(buf->merge.lc_state));
		
                //return lock
                ktau_spin_unlock(buf->hash_lock,kflags);

                // unset the merge-state, to disable merging
                ktau_unmap_userbuf(current->pid, lc_merge.pages, lc_merge.nr_pages, lc_merge.orig_kaddr);

                ret_value = 0;
        }

        //if the request is to UNSET, just do nothing clear the state (as things werealready UNSET above)
        if(pstate == NULL) {

                //everything already done

        } else if (pstate != NULL) {

                //if request is to SET, then set accordingly

                //dont need to take the lock as we are 1st using local (lc_merge)
                if((ret_value = ktau_map_userbuf(current, (unsigned long) pstate, 4096, &lc_merge.pages, &lc_merge.nr_pages, &lc_merge.orig_kaddr, &lc_merge.uptr))) {
                        err("do_ktau_merge: ktau_map_userbuf ret error.\n");
                        return ret_value;
                }

                info("do_ktau_merge: ktau_map_userbuf success. pages:%x nr_pages:%d orig_kaddr:%x uptr:%x", lc_merge.pages, lc_merge.nr_pages, lc_merge.orig_kaddr, lc_merge.uptr);

                ktau_spin_lock(buf->hash_lock,kflags);

                //info("ktau_hash.c:: set_hash_userstate: ppstate: %u\n",ppstate);
		
                //FIX MERGE
		memset(&(lc_merge.lc_state), 0, sizeof(lc_merge.lc_state));
		
                //We allow setting ppstate to NULL (unset)
                buf->merge = lc_merge;

                //info("ktau_hash.c:: set_hash_userstate: assigned: ktau->ppstate: %u\n",
                //                                      buf->ppstate);

                ret_value = 0;

                ktau_spin_unlock(buf->hash_lock,kflags);
        }

        return ret_value;
}

int trigger_mergepoint(struct task_struct* task) {
        ktau_prof* buf;
        ktau_state* pstate;
        if(task->ktau) {
                buf = task->ktau;
                if(buf->merge.uptr) {
                        pstate = (ktau_state*)(buf->merge.uptr);
                        (*pstate) = buf->merge.lc_state;
                }
        }
        return 0;
}

int update_mergestate(ktau_user* pmerge, unsigned long long incl, unsigned long long excl, unsigned long long incr, unsigned int grp) {

        //ktau_state* pstate = NULL;
        volatile int active_index = 0;
        int grp_index = 0;

        info("update_mergestate: pmerge: %u\n", pmerge);

        //handle to user-state
        //NOPE dont use this - only lc_state should be changed
        //pstate = (ktau_state*)pmerge->uptr;

        info("update_mergestate: got pstate: %x\n", pstate);

        //none of this loading dummy etc stuff. Now we just do it directly - not atomic, but long longs themselves

        //get the active index value
        //active_index = pstate->active_index; (assume zero for this version - no double buffer)

        //info("update_mergestate: got active_index: %d\n", active_index);

        //grp specific merge state now - so get the group index
        grp_index = ktau_leftmost1(grp) + 1;
        if(grp_index >= merge_max_grp) {
                err("update_mergestate: Bad grp_index:%d merge_max_grp:%d\n",grp_index, merge_max_grp);
                return 0;
        }

        (pmerge->lc_state.state[active_index]).ktime[grp_index] += incl;
        (pmerge->lc_state.state[active_index]).kexcl[grp_index] += excl;
        (pmerge->lc_state.state[active_index]).knumcalls[grp_index] += incr;

        //and that should finish it up.

        return 0;
}

/* Usnign double buffer strategy - and no local copy */
/*
int update_mergestate(ktau_user* pmerge, unsigned long long incl, unsigned long long excl, unsigned long long incr, unsigned int grp) {

        ktau_state* pstate = NULL;
        volatile int active_index = 0;
        int grp_index = 0;

        info("update_mergestate: pmerge: %u\n", pmerge);

        //handle to user-state
        pstate = (ktau_state*)pmerge->uptr;
        
        info("update_mergestate: got pstate: %x\n", pstate);

        //none of this loading dummy etc stuff. Now we just do it directly - not atomic, but long longs themselves

        //get the active index value
        active_index = pstate->active_index;

        info("update_mergestate: got active_index: %d\n", active_index);

        //grp specific merge state now - so get the group index
        if(grp) {
                grp_index = ktau_leftmost1(grp) + 1;
	}

	//store our local-state into the user-state at active_index
	(pstate->state[active_index]).ktime[grp_index] += incl;
	(pstate->state[active_index]).kexcl[grp_index] += excl;
	(pstate->state[active_index]).knumcalls[grp_index] += incr;

        //and that should finish it up.

        return 0;
}
*/

/* old copy - currently in cvs version of merging */
/*
int update_mergestate(ktau_user* pmerge, unsigned long long incl, unsigned long long incr) {

        ktau_state* pstate = NULL;
	volatile int active_index = 0;

        info("update_mergestate: pmerge: %u\n", pmerge);

	//handle to user-state
        pstate = (ktau_state*)pmerge->uptr;
	
	info("update_mergestate: got pstate: %x\n", pstate);

	//none of this loading dummy etc stuff. Now we just do it directly - not atomic, but long longs themselves

	//get the active index value
	active_index = pstate->active_index;

	info("update_mergestate: got active_index: %d\n", active_index);

	//store our local-state into the user-state at active_index
	(pstate->state[active_index]).ktime += incl;
	(pstate->state[active_index]).knumcalls += incr;

	//and that should finish it up.

        return 0;
}
*/

asmlinkage long sys_ktau_gettimeofday(struct timeval *tv, struct timezone *tz)
{
#ifdef CONFIG_KTAU_MERGE
        unsigned long merge_flag = 0;
#endif /* CONFIG_KTAU_MERGE */
        if (tv) {
                struct timeval ktv;
#ifdef CONFIG_KTAU_MERGE
                local_irq_save(merge_flag);
                trigger_mergepoint(current);
#endif /* CONFIG_KTAU_MERGE */
                do_gettimeofday(&ktv);
#ifdef CONFIG_KTAU_MERGE
                local_irq_restore(merge_flag);
#endif /* CONFIG_KTAU_MERGE */
                if (copy_to_user(tv, &ktv, sizeof(ktv)))
                        return -EFAULT;
        }
        if (tz) {
                if (copy_to_user(tz, &sys_tz, sizeof(sys_tz)))
                        return -EFAULT;
        }
        return 0;
}


