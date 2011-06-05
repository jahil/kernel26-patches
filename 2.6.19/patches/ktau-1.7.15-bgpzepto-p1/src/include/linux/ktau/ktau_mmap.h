/******************************************************************************
 * Version	: $Id: ktau_mmap.h,v 1.2.2.2 2007/03/24 00:09:21 anataraj Exp $
 * ***************************************************************************/ 

#ifndef _KTAU_MMAP_H
#define _KTAU_MMAP_H

/* generic ktau headers */
#include <linux/ktau/ktau_datatype.h>

/* ktau_proc headers */
#include <linux/ktau/ktau_proc_data.h>		/* all struct defs */
#include <linux/ktau/ktau_proc_external.h> 	/* Info shared with user-lib */

#include <asm/uaccess.h>			/* for access_ok & VERIFY_WRITE */

/* declarations to avoid #including kernel headers here */
struct task_struct;
struct page;

//int copy_collapsed(char* buf, ktau_output* data, unsigned int noprofs, unsigned int hdr_size);

#ifdef CONFIG_KTAU_TRACE
//int copy_collapsed_trace(char* buf, ktau_output* data, unsigned int noprofs, unsigned int hdr_size);
#endif /* CONFIG_KTAU_TRACE */


/* ktau_validate_umem:
 * Validates the user-memory passed into kernel.
 * This is usually only required for sharing bet.
 * kernel and user.
 * TODO: Must find a better, way to do this.
 *	 Currently just doing range-ok.
 *
 * [in] uaddr	: the address of mem buf passed from user.
 * [in] size	: size of mem buf from user.
 *
 * Returns:
 * 0 on Sucess, otherwise error.
 *
 */
static inline int ktau_validate_umem(void* uaddr, unsigned long usize) {
	return (!access_ok(VERIFY_WRITE, uaddr, usize));
}


/* ktau_map_userbuf:
 * Maps the user-space buffer into
 * kernel address-space using get_user_pages and
 * vmap.
 * 
 * [in] task	:	task_struct (usually current)
 * [in] uaddr	:	user-space address of buffer
 * [in] size	:	size of user-buffer
 * [inout] pages:	to be allocated to hold pages	
 * [inout] pgaddr:	to be allocated to hold pages	
 * 
 * Return	:	kernel-space address to buffer
 *---------------------------------------------------*/
int ktau_map_userbuf(struct task_struct* task, unsigned long uaddr, unsigned long size, struct page*** p_pages, int* pnr_pages, unsigned long* porig_kaddr, unsigned long* pkaddr);


/* ktau_unmap_userbuf:
 * Un-maps the user-space buffer 
 * from kernel address-space using vunmap and
 * page_cache_release.
 * 
 * [in] taskpid	:	task pid (usually current) - debuging only
 * [in] pages	:	to be de-allocated 
 * [in] nr_pages:	no of pages to be dealloc
 * [in] orig_kaddr:	mapped kernel address
 *
 * Return	:	none
 *---------------------------------------------------*/
void ktau_unmap_userbuf(pid_t taskpid, struct page** pages, int nr_pages, unsigned long orig_kaddr);

#endif /* _KTAU_MMAP_H */
