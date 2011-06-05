/******************************************************************************
 * File 	: 
 * Version	: $Id: ktau_mmap.c,v 1.2.2.4 2007/04/06 21:12:29 anataraj Exp $
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
#define TAU_NAME "ktau_mmap" 
#include <linux/ktau/ktau_print.h>
#include <linux/ktau/ktau_datatype.h>
#include <linux/ktau/ktau_api.h>

/* ktau_proc headers */
#include <linux/ktau/ktau_proc_data.h>		/* all struct defs */
#include <linux/ktau/ktau_proc.h>		/* Interfaces used by ktau */
#include <linux/ktau/ktau_proc_external.h> 	/* Info shared with user-lib */

/* Do we use kmalloc OR vmalloc? */
//#define USE_KMALLOC 1
#define USE_VMALLOC 1

/* to easily switch between kmalloc and vmalloc */
#ifdef USE_KMALLOC
#define KERN_MALLOC_STR		"kmalloc"
#define KERN_MALLOC(x)		kmalloc((x), GFP_KERNEL)	
#define KERN_FREE(x)		kfree((x))
#else /* USE_KMALLOC */
#define KERN_MALLOC_STR		"vmalloc"
#define KERN_MALLOC(x)		vmalloc((x))	
#define KERN_FREE(x)		vfree((x))
#endif /* USE_KMALLOC */


/*
int copy_collapsed(char* buf, ktau_output* data, unsigned int noprofs, unsigned int hdr_size)
{
	int i = 0;
	char *bufptr = NULL;

	if(hdr_size < 0)
		hdr_size = 0;

	bufptr = buf; //start

	memcpy(bufptr, &noprofs, sizeof(unsigned int)); //noprofs
	bufptr += (sizeof(unsigned int));
	for(i = 0; i< noprofs; i++)
	{
		memcpy(bufptr, &(data[i].pid), sizeof(pid_t));//pid
		bufptr += (sizeof(unsigned int));
		memcpy(bufptr, &(data[i].size), sizeof(unsigned int));//size
		bufptr += (sizeof(unsigned int));
		memcpy(bufptr, data[i].ent_lst, sizeof(o_ent) * data[i].size);
		bufptr += (sizeof(o_ent)*data[i].size);
	}
	
	return (bufptr - buf); // total copied-size 
}
*/

#ifdef CONFIG_KTAU_TRACE
/*
int copy_collapsed_trace(char* buf, ktau_output* data, unsigned int noprofs, unsigned int hdr_size)
{
	int i = 0;
	char *bufptr = NULL;

	if(hdr_size < 0)
		hdr_size = 0;

	info("copy_collapse: req. hdr_size: %u\n", hdr_size);

	bufptr = buf; //start

	memcpy(bufptr, &noprofs, sizeof(unsigned int)); //noprofs
	bufptr += (sizeof(unsigned int));
	for(i = 0; i< noprofs; i++)
	{
		memcpy(bufptr, &(data[i].pid), sizeof(pid_t));//pid
		bufptr += (sizeof(unsigned int));
		memcpy(bufptr, &(data[i].size), sizeof(unsigned int));//size
		bufptr += (sizeof(unsigned int));
		memcpy(bufptr, data[i].trace_lst, sizeof(ktau_trace) * data[i].size);
		bufptr += (sizeof(ktau_trace)*data[i].size);
	}
	
	return (bufptr - buf); // total copied-size 
}
*/
#endif /* CONFIG_KTAU_TRACE */


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
 * Return	:	return code( ZERO success )
 *****************************************************/

int ktau_map_userbuf(struct task_struct* task, unsigned long uaddr, unsigned long size, struct page*** p_pages, int* pnr_pages, unsigned long* porig_kaddr, unsigned long* pkaddr) {
        int res = 0, pgno = 0, nr_pages = 0; 
        struct page *page = NULL;
        unsigned long offset = 0, endoffset = 0, retval = 0;

	info("ktau_map_userbuf: enter: task->pid:%d  uaddr:%lx  size:%lu  pages:%p\n", task->pid, uaddr, size, *p_pages);

	offset = uaddr & (PAGE_SIZE-1); /* uaadr may not begin at page-start */
	nr_pages = ((size + offset) >> PAGE_SHIFT) + 1;
	endoffset = PAGE_SIZE - offset;

	info("ktau_map_userbuf: page_size-1: %d  offset: %lu  endoffset: %lu  nr_pages: %d  \n", PAGE_SIZE-1, offset, endoffset, nr_pages);

	*p_pages = (struct page**)KERN_MALLOC((nr_pages)*sizeof(struct page*));
	if(!(*p_pages)) {
		err("ktau_map_userbuf: " KERN_MALLOC_STR " failed.\n");
		retval = -ENOMEM;
		goto out_error;
	}

	/* take the mmap lock, then get the pages,
	 * then release the lock.
	 * May sleep here - its a semaphore.
	 */
	down_read(&current->mm->mmap_sem);
	res = get_user_pages(
		current,        /* task_struct */
		current->mm,    /* mem context */
		uaddr,          /* user-space addr */
		nr_pages,       /* no of pages */
		1,              /* Rw flag ?? */
		0,              /* dont force ?? */
		*p_pages,          /* output arr */
		NULL            /* vma**, dont need it */
	);
	up_read(&current->mm->mmap_sem);
	if(res < nr_pages) {
		err("ktau_map_userbuf: get_user_pages error: res: %d < nr_pages: %d\n", res, nr_pages);
		retval = -ENOMEM;
		goto out_unmap;
	}

#ifdef TAU_DEBUG
	for (pgno=0; pgno < nr_pages; pgno++) {
		page = (*p_pages)[pgno];

		info("ktau_map_userbuf: After get_user_pages: page: %d]. \
			Resrv:%d Err:%d Refr:%d Actv:%d Lock:%d Dirty:%d Pgcnt:%d\n", \
		 	pgno, PageReserved(page), PageError(page), PageReferenced(page), \
			PageActive(page), PageLocked(page), \
			PageDirty(page), page_count(page));
	}
#endif /* TAU_DEBUG */

	*porig_kaddr = (unsigned long) vmap(*p_pages, nr_pages, VM_ALLOC, PAGE_KERNEL);
	if(!(*porig_kaddr)) {
		err("ktau_map_userbuf: vmap failed.\n");
		retval = -ENOMEM;
		goto out_unmap;
	}
	*pkaddr = *porig_kaddr + offset; /* 1st page's kaddr may not start at page-start */

	info("ktau_map_userbuf: vmap offseted kaddr: %lx\n", *pkaddr);

#ifdef TAU_DEBUG
	for (pgno=0; pgno < nr_pages; pgno++) {
		page = (*p_pages)[pgno];

		info("ktau_map_userbuf: After vmap: page: %d]. \
			Resrv:%d Err:%d Refr:%d Actv:%d Lock:%d Dirty:%d Pgcnt:%d\n", \
		 	pgno, PageReserved(page), PageError(page), PageReferenced(page), \
			PageActive(page), PageLocked(page), \
			PageDirty(page), page_count(page));
	}
#endif /* TAU_DEBUG */

	/* getting out normally now */
	*pnr_pages = nr_pages;
	info("ktau_map_userbuf: exit\n");
	return 0;

out_unmap:
	if (res > 0) {
		for (pgno=0; pgno < res; pgno++) {
			page = (*p_pages)[pgno];
			info("ktau_map_userbuf: page: %d]. \
				Resrv:%d Err:%d Refr:%d Actv:%d Lock:%d Dirty:%d Pgcnt:%d\n", \
			 	pgno, PageReserved(page), PageError(page), PageReferenced(page), \
				PageActive(page), PageLocked(page), \
				PageDirty(page), page_count(page));

			page_cache_release(page);
		}
	}
	/* try to free alloc mem */
	if(*p_pages) KERN_FREE(*p_pages);

out_error:
	*pnr_pages = 0;
	*porig_kaddr = 0;
	*pkaddr = 0;
	err("ktau_map_userbuf: error exit\n");
	return retval;
}


/* ktau_unmap_userbuf:
 * Un-maps the user-space buffer 
 * from kernel address-space using vunmap and
 * page_cache_release.
 * 
 * [in] taskpid	:	pid (usually current) - only for debug
 * [in] pages	:	to be de-allocated 
 * [in] nr_pages:	no of pages to be dealloc
 * [in] orig_kaddr:	mapped kernel address
 *
 * Return	:	none
 *****************************************************/

void ktau_unmap_userbuf(pid_t taskpid, struct page** pages, int nr_pages, unsigned long orig_kaddr) {
	int pgno = 0;
	struct page *page = NULL;

	info("ktau_unmap_userbuf: enter: taskpid:%d  orig_kaddr:%lx  pages:%p  nr_pgs:%d\n", taskpid, orig_kaddr, pages, nr_pages);

	/* unmap the previously mapped mem out of kernel-space */
	vunmap((void*)orig_kaddr);

	for (pgno=0; pgno < nr_pages; pgno++) {
		page = pages[pgno];

		info("ktau_unmap_userbuf: BEFOR SetPageDirty: page: %d]. \
			Resrv:%d Err:%d Refr:%d Actv:%d Lock:%d Dirty:%d Pgcnt:%d\n", \
		 	pgno, PageReserved(page), PageError(page), PageReferenced(page), \
			PageActive(page), PageLocked(page), \
			PageDirty(page), page_count(page));

		if(!PageReserved(page)) {
			SetPageDirty(page); //we are using this memory to write to uspace - so it must be dirty.
		}

		info("ktau_unmap_userbuf: BEFOR page_cache_release: page: %d]. \
			Resrv:%d Err:%d Refr:%d Actv:%d Lock:%d Dirty:%d Pgcnt:%d\n", \
		 	pgno, PageReserved(page), PageError(page), PageReferenced(page), \
			PageActive(page), PageLocked(page), \
			PageDirty(page), page_count(page));

		/* undo the get_user_pages effects */
		page_cache_release(page);

		info("ktau_unmap_userbuf: After page_cache_release: page: %d]. \
			Resrv:%d Err:%d Refr:%d Actv:%d Lock:%d Dirty:%d Pgcnt:%d\n", \
		 	pgno, PageReserved(page), PageError(page), PageReferenced(page), \
			PageActive(page), PageLocked(page), \
			PageDirty(page), page_count(page));

	}

	/* try to free alloc mem */
	if(pages) KERN_FREE(pages);

	info("ktau_unmap_userbuf: exit");

	return;
}


