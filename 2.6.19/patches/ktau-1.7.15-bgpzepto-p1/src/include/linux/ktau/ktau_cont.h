/******************************************************************************
 * Version	: $Id: ktau_cont.h,v 1.1.2.10 2007/03/19 07:35:09 anataraj Exp $
 * ***************************************************************************/ 

#ifndef _KTAU_CONT_H
#define _KTAU_CONT_H

/* declarations to avoid #including kernel headers here */
struct task_struct;

struct _ktau_prof;

#include <linux/ktau/ktau_cont_data.h>

/* ktau_share_cont:
 * Maps the user-space shcont into
 * kernel address-space using ktau_mmap_userbuf.
 * Then it initializes the ktau_shcont and registers
 * it with the task_struct (current).
 * Increments reference count.
 * 
 * [in] task	:	task_struct (usually current)
 * [inout] ushcont:	ptr to ktau_ushcont 
 *			(a copy of the one from uspace)
 * 
 * Return	:	returns ptr to new ktau_shcont 
 *			or NULL.
 *---------------------------------------------------*/
ktau_shcont* ktau_share_cont(struct task_struct* task, ktau_ushcont* ushcont);

/* ktau_unshare_cont:
 * Unmaps the user-space shcont from
 * kernel address-space using ktau_unmap_userbuf.
 * Before that it unregisters the shcont from 
 * task_ctruct (usually current).
 * 
 * [in] task	:	task_struct (usually current)
 * [in] ushcont	:	ptr to ktau_ushcont 
 *			(a copy of the one from uspace)
 * 
 * Return	:	(int) 0 on Success. -ve on error.
 *
 * Constraints	:	Reference count of shcont MUST
 *			be 1 when this is called. 
 *			If ref_cnt > 1, then some object
 *			(e.g. counter) is still residing
 *			in the container.
 *			Then, return -(ref_cnt).
 *
 *---------------------------------------------------*/
int ktau_unshare_cont(struct task_struct* task, ktau_ushcont* ushcont);


/* ktau_ctr_create:
 * Creates a shared counter, associates it with
 * the requested sh-container, registers all the
 * events that the counter expresses interest in.
 * Increments reference count of shared-container.
 * Sets the k_id of the counter (so that it can be
 * located later by calls from uspace calls).
 * 
 * [in] task	:	task_struct (usually current)
 * [inout] u_shctr:	ptr to ktau_sh_ctr  
 *			(a copy of the one from uspace
 *			 recieved thro /proc).
 * [in]	kid	:	the id of the req. sh-container
 * Return	:	0 on SUCESS.
 *			-ve on error. (error codes TODO).
 *---------------------------------------------------*/
int ktau_ctr_create(struct task_struct* task, ktau_ush_ctr* u_shctr, int shcont_kid);

/* ktau_ctr_delete:
 * finds the kernel-counter from the k_id, 
 * deletes a shared counter, dissociates it from
 * its sh-container, unregisters all the
 * events that the counter expressed interest in.
 * Decrements reference count of shared-container.
 * 
 * [in] task	:	task_struct (usually current)
 * [inout] u_shctr:	ptr to ktau_sh_ctr  
 *			(a copy of the one from uspace
 *			 recieved thro /proc).
 *
 * Return	:	0 on SUCESS.
 *			-ve on error. (error codes TODO).
 *---------------------------------------------------*/
int ktau_ctr_delete(struct task_struct* task, ktau_ush_ctr* u_shctr);


/* ktau_shctr_handler_def:
 * Default shared-counter event handler.
 * Returns 0 on success. -ve on error.
 * Usually called from startr/stop inst. routines
 */
int ktau_shctr_handler_def(ktau_trigger* ev, int type, unsigned long long incl, unsigned long long excl);


/* ktau_shctr_gc:
 * Garbage collect shared-counter state on process-death-without-cleanup.
 *
 * Return:
 * On success Zero, otherwise -ve.
 * 
 * Note: DONT hold lock and call this.
 */
int ktau_shctr_gc(pid_t pid, struct _ktau_prof* prof);


/* ktau_shcont_gc:
 * Garbage collect shared-container state on process-death-without-cleanup.
 *
 * Return:
 * On success Zero, otherwise -ve.
 *
 * Note: Assumes that container(s) not being used.
 * It will do the GC even
 * if ref_cnt of container's are non-zero.
 * So call ktau_shctr_gc before calling this.
 *
 * Note: DONT hold lock and call this.
 */
int ktau_shcont_gc(pid_t pid, struct _ktau_prof* prof);


#endif /* _KTAU_CONT_H */
