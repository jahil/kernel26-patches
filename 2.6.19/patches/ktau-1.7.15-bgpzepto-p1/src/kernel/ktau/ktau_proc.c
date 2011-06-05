/******************************************************************************
 * File 	: kernel/ktau/ktau_proc.c
 * Version	: $Id: ktau_proc.c,v 1.8.2.11 2008/11/19 05:26:50 anataraj Exp $
 * ***************************************************************************/ 

/* to enable debug messages */
#define TAU_DEBUG

/* kernel headers */
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <asm/current.h>
#include <asm/uaccess.h>
//#include <linux/slab.h>
//#include <linux/mm.h>
//#include <linux/pagemap.h>
#include <linux/vmalloc.h>	/* for vmalloc */

/* generic ktau headers */
#define TAU_NAME "ktau_proc" 
#include <linux/ktau/ktau_print.h>
#include <linux/ktau/ktau_datatype.h>
#include <linux/ktau/ktau_api.h>

#include <linux/ktau/ktau_bootopt.h>

/* ktau_proc headers */
#include <linux/ktau/ktau_proc_data.h>		/* all struct defs */
#include <linux/ktau/ktau_proc.h>		/* Interfaces used by ktau */
#include <linux/ktau/ktau_proc_external.h> 	/* Info shared with user-lib */
#include <linux/ktau/ktau_proc_int.h> 		/* intrernal headers for use only by ktau_proc */
#include <linux/ktau/ktau_mmap.h> 		/* memory management & buf copy functions */
#include <linux/ktau/ktau_cont.h> 		/* Shared container api */
#include <linux/ktau/ktau_inject.h> 		/* Ktau Noise Injection */

#ifdef CONFIG_ZEPTO_MEMORY
#include <linux/compclass.h>			/* Zepto Compute Class checking */
#endif /* CONFIG_ZEPTO_MEMORY */


//#define PROFILE_ALLOC(SIZE)	kmalloc((SIZE),GFP_KERNEL)
//#define PROFILE_FREE(PTR)	kfree((PTR))
#define PROFILE_ALLOC(SIZE)	vmalloc((SIZE))
#define PROFILE_FREE(PTR)	vfree((PTR))

//#define TRACE_ALLOC(SIZE)	kmalloc((SIZE),GFP_KERNEL)
//#define TRACE_FREE(PTR)	kfree((PTR))
#define TRACE_ALLOC(SIZE)	vmalloc((SIZE))
#define TRACE_FREE(PTR)		vfree((PTR))

/* KTAU PROC Command function table *
 *----------------------------------*/
#ifdef CONFIG_KTAU_TRACE
CMD_FUNCPTR ktau_user_cmds[KTAU_MAX_CMD][KTAU_MAX_TYPE] = \
						{ \
							/* cmd-0 size */ \
							{ &cmd_size_profile, &cmd_size_trace, &cmd_noop } , \
							/* cmd-1 read */ \
							{ &cmd_read_profile, &cmd_read_trace, &cmd_noop } , \
							/* cmd-2 merge */ \
							{ &cmd_merge_profile, &cmd_noop } , \
							/* cmd-3 re-size */ \
							{ &cmd_noop, &cmd_resize_trace, &cmd_noop } , \
							/* cmd-4 add-shcont */ \
							{ &cmd_noop, &cmd_noop } , \
							/* cmd-5 del-shcont */ \
							{ &cmd_noop, &cmd_noop } , \
							/* cmd-6 add-shctr */ \
							{ &cmd_noop, &cmd_noop } , \
							/* cmd-7 del-shctr */ \
							{ &cmd_noop, &cmd_noop } , \
							/* cmd-8 inject-noise */ \
							{ &cmd_noop, &cmd_noop } , \
							/* no-ops */ \
							{ &cmd_noop } \
						};
#else /* CONFIG_KTAU_TRACE */
/* the same table above - but without the trace-functions */
CMD_FUNCPTR ktau_user_cmds[KTAU_MAX_CMD][KTAU_MAX_TYPE] = \
						{ \
							/* cmd-0 size */ \
							{ &cmd_size_profile, &cmd_noop } , \
							/* cmd-1 read */ \
							{ &cmd_read_profile, &cmd_noop } , \
							/* cmd-2 merge */ \
							{ &cmd_merge_profile, &cmd_noop } , \
							/* cmd-3 re-size */ \
							{ &cmd_noop } , \
							/* cmd-4 add-shcont */ \
							{ &cmd_addshcont_profile, &cmd_noop } , \
							/* cmd-5 del-shcont */ \
							{ &cmd_delshcont_profile, &cmd_noop } , \
							/* cmd-6 add-shctr */ \
							{ &cmd_addshctr_profile, &cmd_noop } , \
							/* cmd-7 del-shctr */ \
							{ &cmd_delshctr_profile, &cmd_noop } , \
							/* cmd-7 inject-noise */ \
							{ &cmd_inject_profile, &cmd_noop } , \
							/* no-ops */ \
							{ &cmd_noop } \
						};
#endif /* CONFIG_KTAU_TRACE */


/* 
 * ktau_fops : file_operations sturcture holding the ioctl func ptr *
 *------------------------------------------------------------------*/
static struct file_operations ktau_fops = {
        ioctl:          ktau_ioctl_proc
};

/*-------------- /proc/ktau/ init ----------------------*/
/*
 *  This is the initialize function which is called
 *   by start_kernel() in init/main.c.
 */
static struct proc_dir_entry *proc_ktau;

void __init ktau_init(void)
{
#ifndef CONFIG_KTAU_TRACE
        struct proc_dir_entry *proc_ktau_profile;
#else /* CONFIG_KTAU_TRACE */
	struct proc_dir_entry *proc_ktau_trace;
#endif /*CONFIG_KTAU_TRACE */
        /* Create /proc/ktau/ */
        if((proc_ktau = proc_mkdir(TAU_PROC_ROOT, NULL))){
                proc_ktau->mode = S_IFDIR|0755;
                printk(KERN_INFO "ktau_init: Success setup /proc/ktau\n");
        } else {
                printk(KERN_INFO "ktau_init: FAILED setup proc ktau\n");
        }

#ifndef CONFIG_KTAU_TRACE
        /* Create /proc/ktau/profile */
        if((proc_ktau_profile = create_proc_entry("profile",0444,proc_ktau))){
                proc_ktau_profile->data = NULL;

                ktau_fops = *(proc_ktau_profile->proc_fops);
                ktau_fops.ioctl = ktau_ioctl_proc;
                
                proc_ktau_profile->proc_fops = &ktau_fops;
                printk(KERN_INFO "ktau_init: Success setup /proc/ktau/profile\n");
        } else {
                err("ktau_init: create_proc_entry failed.");
        }
#endif /*CONFIG_KTAU_TRACE */

#ifdef CONFIG_KTAU_TRACE
#ifdef CONFIG_KTAU_BOOTOPT
	if(ktau_verify_bootopt(KTAU_TRACE_MSK))
#endif /* CONFIG_KTAU_BOOTOPT*/
	{
		/* Create /proc/ktau/trace */
		if((proc_ktau_trace = create_proc_entry("trace",0444,proc_ktau))){
			proc_ktau_trace->data = NULL;

			/* We cannot say proc_ktau_trace->proc_fops->ioctl = ktau_ioctl_proc	*
			 * Becoz a single proc_fops is shared by all /proc entries		* 
			 * if we change that, then all /proc ioctls come to us.    		*
			 * Instead we create a new fops and point our /proc/ktau entry's fop 	*
			 * to it. 								*
			 * ktau_fops is global (not on the stack) so its lifetime is forever.	*/

			ktau_fops = *(proc_ktau_trace->proc_fops);
			ktau_fops.ioctl = ktau_ioctl_proc;
			
			proc_ktau_trace->proc_fops = &ktau_fops;
			printk(KERN_INFO "ktau_init: Success setup proc/ktau/trace\n");
		} else {
			err("ktau_init: create_proc_entry failed.");
		}
	}
#endif /*CONFIG_KTAU_TRACE */

        /* Initialize the global semaphore */
        ktau_global_semaphore_init();

	printk(KERN_INFO "ktau_kernel_init: DONE!!! \n");
}

#if 0
/*--------------- /proc/ktau/<pid> setup and destroy ---------------------*/
/* 
 * To be used to create a new /proc/ktau/<pid> 
 */
void setup_proc_ktau(struct task_struct* tsk)
{
        struct proc_dir_entry* file_ptr;
        char _tau_tmp_proc_name[TAU_PROC_MAX_NAME];

        //snprintf(_tau_tmp_proc_name, TAU_PROC_MAX_NAME, "%s/%d",
        //                TAU_PROC_ROOT, tsk->pid);
        snprintf(_tau_tmp_proc_name, TAU_PROC_MAX_NAME, "%d",
                        tsk->pid);

        //file_ptr = create_proc_entry(_tau_tmp_proc_name, 0444, NULL);
        file_ptr = create_proc_entry(_tau_tmp_proc_name, 0444, proc_ktau);
	if(!file_ptr)
	{
		err("setup_proc_tau: create_proc_entry failed.\n");
		return;
	}

        file_ptr->data = tsk;
        file_ptr->write_proc = ktau_write_proc;
        file_ptr->proc_fops = &ktau_fops;

	return;
}


/* 
 * To be used to destroy an existing /proc/ktau/<pid> 
 */
void destroy_proc_ktau(int pid)
{
        char _tau_tmp_proc_name[TAU_PROC_MAX_NAME];
        //snprintf(_tau_tmp_proc_name, TAU_PROC_MAX_NAME, "%s/%d",
        //                TAU_PROC_ROOT, pid);
        snprintf(_tau_tmp_proc_name, TAU_PROC_MAX_NAME, "%d",
                        pid);
        //printk(KERN_INFO "destroy_proc_tau: destroying %s\n", _tau_tmp_proc_name);
        
	//remove_proc_entry(_tau_tmp_proc_name, NULL);
	remove_proc_entry(_tau_tmp_proc_name, proc_ktau);

}
#endif /* 0 - setup/destroy not used */

/*------------- IOCTL FUNCTION ------------------------------*
 *-----------------------------------------------------------*/
int ktau_ioctl_proc(struct inode *inode,
			  struct file *file, 
			  unsigned int cmd, 
			  unsigned long arg)
{                               
	int retval = 0, nopids = 0, nr_pages = 0, ubuf_size = 0;
	unsigned long not_copied = 0, orig_kaddr = 0, kaddr = 0;
	pid_t* pidlist = NULL;
	struct page **pages = NULL;
	ktau_query query;

	info("ktau_ioctl_proc: Enter [current->pid: %d, Cmd: %x]\n", (current)->pid, cmd);

	/* Step 0: Check if this is a timing no-op and if so just bail. */
	if(cmd == KTAU_PROC_MAGIC_NOOP) {
		/* basic timing request */
		return 0;
	}
	
	/* Step 1: Validate KTAU_PROC_MAGIC */
	if(cmd != KTAU_PROC_MAGIC) {
		/* invalid request */
		err("ktau_ioctl_proc: bad magic:%x  Aborting.", cmd);
		return -ENOTTY;
	}

	/* Step 2: ok magic is fine, now get the query from user-land */
	not_copied = copy_from_user((char*)&query, (char*)arg, sizeof(query));
	if(not_copied != 0) 
	{
		err("ktau_ioctl_proc: copy_form_user(query) failed. uncopied : %lu. Aborting.\n", not_copied);
		return -ENOTTY;
	}

	info("ktau_ioctl_proc: Query is: Type:%x, SubType:%x, Cmd:%x, SubCmd:%x, Target:%x, pidlist:%p, nopids:%d\n", KTAU_GET_TYPE(query.bitflag), KTAU_GET_SUBTYPE(query.bitflag), KTAU_GET_CMD(query.bitflag), KTAU_GET_SUBCMD(query.bitflag), query.target, query.pidlist, query.nopids);

	/* Step 3: Validate the query */
	if(validate_query(&query)) {
		err("ktau_ioctl_proc: validate_query failed. Aborting.\n");
		return -ENOTTY;
	}

	/* Ok basic query validation done. */

	/* Now find out pids & nopids */
	if(query.target == KTAU_TARGET_ALL) {
		pidlist = NULL;	
		nopids = 0;
	} else if(query.target == KTAU_TARGET_SELF) {
		pidlist = NULL;	
		nopids = 1;
	} else if((query.target == KTAU_TARGET_MANY) ||(query.target == KTAU_TARGET_PID)) {
			if(get_usrpidlist(&pidlist, query.pidlist, query.nopids) != 0) {
				err("ktau_ioctl_proc: get_usrpidlist failed. Aborting.\n");
				return -ENOTTY;
			}
	} //else should have been caught as invalid in validation

	/* Check if we need to get the user-memory mmaped into k-addr *
	 * This we only do for read operations currently 		  */
	if(KTAU_GET_CMD(query.bitflag) == KTAU_CMD_READ) {
#ifdef CONFIG_ZEPTO_MEMORY
		if(IS_COMPCLASS(current)) {
			orig_kaddr = kaddr = (unsigned long)query.op.read.buf;
		} else 
#endif /* CONFIG_ZEPTO_MEMORY */
		{
			retval = ktau_map_userbuf(current, (unsigned long)query.op.read.buf, query.op.read.size, &pages, &nr_pages, &orig_kaddr, &kaddr);
			if((retval < 0) || (kaddr == 0)) {
				err("ktau_ioctl_proc: ktau_map_userbuf ret err.\n");
				return -ENOMEM;
			}																		
		}
		ubuf_size = query.op.read.size;
	}


	/* Step 4: Now the actual work. Call the appropriate CMD function */
	retval = (*ktau_user_cmds[KTAU_GET_CMD(query.bitflag)][KTAU_GET_TYPE(query.bitflag)])(&query, pidlist, query.nopids, (char*)kaddr, ubuf_size);
	if(retval < 0) {
		err("ktau_iotcl_proc: ktau_user_cmd ret error. Cmd:%x , Type:%x\n", KTAU_GET_CMD(query.bitflag), KTAU_GET_TYPE(query.bitflag));
	}

	/* if we mapped-in user-pages, we unmap them out */
	if((KTAU_GET_CMD(query.bitflag) == KTAU_CMD_READ) && (orig_kaddr!=0)) {
#ifdef CONFIG_ZEPTO_MEMORY
		if(!IS_COMPCLASS(current))
#endif /* CONFIG_ZEPTO_MEMORY */
		{
			ktau_unmap_userbuf(current->pid, pages, nr_pages, orig_kaddr);
		}
	}

	/* write the query(with changed result information) back to user-land.	 *
	 * Note: This doesnt write the actual output data - only the query-hdr.	 *
	 * The actual data is transferred from within the cmds[][] functions.	 */
	if(arg) {
		not_copied = copy_to_user((char*)arg, (char*)&query, sizeof(query));
		if(not_copied != 0) 
		{
			err("ktau_ioctl_proc: copy_to_user(query) failed. uncopied : %lu. Ignoring and returning.\n", not_copied);
		}
	}

	if(pidlist) {
		/* if allocated (i.e not null), then de-allocate */
		put_usrpidlist(pidlist);
		pidlist = NULL;
	}
	
	info("ktau_ioctl_proc: Exit [current->pid: %d, Cmd: %x]\n", (current)->pid, cmd);

	return retval;
}


#ifdef CONFIG_KTAU_TRACE
/* cmd_size_trace:	Performs size calc for trace data.	*
 * [in]	pquery:		ptr to user query			*
 * [in] pidlist:	list of pids				*
 * [in] nopids:		no pids in pidlist			*
 * [inout] buf:		IGNORE					*
 * [in] buf_size:	IGNORE					*
 * Return:	0 on success. Otherwise error			*
 *--------------------------------------------------------------*/
static int cmd_size_trace(ktau_query* pquery, pid_t* pidlist, int nopids, char* buf, unsigned long buf_size) {
	int size_from_trace = 0, ret = 0;
	info("cmd_size_trace: Enter: nopids: %d\n", nopids);

	if(pidlist == NULL && nopids == 1) {
		/* then this is 'self' */
		ret = get_trace_size_self(current, current->ktau, &size_from_trace);
	} else {
		ret = get_trace_size_many(pidlist, nopids, &size_from_trace);
	}

	if(ret <= 0) {
		err("cmd_size_trace: get_trace_size_[self/many]() ret <= 0\n");
		return -1;
	}

	pquery->op.size.size = calc_trace_size(nopids, size_from_trace);

	info("cmd_size_trace: Total Trace Size: %lu\n", pquery->op.size.size);

	return 0;
}
#endif /* CONFIG_KTAU_TRACE */


/* cmd_size_profile:	Performs size calc for profile data.	*
 * [in]	pquery:		ptr to user query			*
 * [in] pidlist:	list of pids				*
 * [in] nopids:		no pids in pidlist			*
 * [inout] buf:		IGNORE					*
 * [in] buf_size:	IGNORE					*
 * Return:	0 on success. Otherwise error			*
 *--------------------------------------------------------------*/
static int cmd_size_profile(ktau_query* pquery, pid_t* pidlist, int nopids, char* buf, unsigned long buf_size) {
	int noentries = 0;

	info("cmd_size_profile: Enter: nopids: %d\n", nopids);

	if(pidlist == NULL && nopids == 1) {
		/* then this is 'self' */
		noentries = get_profile_size_self(current->ktau);
	} else {
		noentries = get_profile_size_many(pidlist, &nopids);
	}

	pquery->op.size.size = calc_profile_size(nopids, noentries);

	info("cmd_size_profile: Exit: nopids found: %d,  noentries:%d,  Total size:%lu\n", nopids, noentries, pquery->op.size.size);

	return 0; 
}


#ifdef CONFIG_KTAU_TRACE
/* cmd_read_trace:	Services user read-op for trace data.	*
 *								*
 * [in]	pquery:		ptr to user query			*
 * [in] pidlist:	list of pids				*
 * [in] nopids:		no pids in pidlist			*
 * [inout] buf:		allocated-buf to write trace into	*
 * [in] buf_size:	size of the buf				*
 *								*
 * Return:	0 on success. Otherwise error			*
 *--------------------------------------------------------------*/
static int cmd_read_trace(ktau_query* pquery, pid_t* pidlist, int nopids, char* buf, unsigned long buf_size) {
	int noprofs = 0;
	unsigned long copy_size = 0;
	ktau_output trace, *ptraces = NULL;

	info("cmd_read_trace: Enter: pidlist:%p,  nopids:%d,  buf:%p,  buf_size:%lu\n", pidlist, nopids, buf, buf_size);

	/* Get the data from ktau_hash */
	if(nopids == 1 && pidlist == NULL) {
		/* This means 'self' */
		ptraces = &trace;
		noprofs = dump_trace_self(current->ktau, ptraces);
	} else { 
		/* one or more others (including 'all')*/ 
		noprofs = dump_trace_many(pidlist, nopids, &ptraces);
	}

	if(noprofs < 0) {
		err("cmd_read_trace: dump_trace_(self/many) ret : %d\n",noprofs);
		return noprofs;
	}

	/* Zero profiles may be returned. This is not an error.		*
	 * For example a process may have died before this call.	*/ 
	if(noprofs == 0) {
		info("cmd_read_trace: dump_trace_(self/many) ret 0 profs.\n");
	} 

	/* Copy that data into the provided buffer in a collapsed form.		*/
	copy_size = copy_collapsed_trace(buf, ptraces, noprofs, KTAU_PROFILE_HDRSIZE);

	if( !(nopids == 1 && pidlist == NULL) ) {
		/* i.e its not self (for evrything else pprofiles is allocated)  */
		free_trace_data(ptraces, noprofs);
	}

	/* Place the proc-header  info into pquery (later get written to uland)	* 
	 * pquery->bitflag and other fields are already fine. 	*
	 * The size field needs to be filled in */
	info("cmd_read_trace: Exit: trace data size:%lu\n", copy_size);
	pquery->op.read.size = copy_size;
	
	return 0;
}
#endif /* CONFIG_KTAU_TRACE */

/* cmd_read_profile:	Services user read-op for profile data.	*
 *								*
 * [in]	pquery:		ptr to user query			*
 * [in] pidlist:	list of pids				*
 * [in] nopids:		no pids in pidlist			*
 * [inout] buf:		allocated-buf to write profile into	*
 * [in] buf_size:	size of the buf				*
 *								*
 * Return:	0 on success. Otherwise error			*
 *--------------------------------------------------------------*/
static int cmd_read_profile(ktau_query* pquery, pid_t* pidlist, int nopids, char* buf, unsigned long buf_size) {
	int noprofs = 0;
	unsigned long copy_size = 0;
	ktau_output profile, *pprofiles = NULL;

	info("cmd_read_profile: Enter: pidlist:%p,  nopids:%d,  buf:%p,  buf_size:%lu\n", pidlist, nopids, buf, buf_size);

	/* Get the data from ktau_hash */
	if(nopids == 1 && pidlist == NULL) {
		/* This means 'self' */
		pprofiles = &profile;
		noprofs = dump_hash_self(current, current->ktau, pprofiles, 0);
	} else { 
		/* one or more others (including 'all')*/ 
		noprofs = dump_hash_many(pidlist, nopids, &pprofiles, 0);
	}

	if(noprofs < 0) {
		err("cmd_read_profile: dump_hash_(self/many) ret : %d\n",noprofs);
		return noprofs;
	}

	/* Zero profiles may be returned. This is not an error.		*
	 * For example a process may have died before this call.	*/ 
	if(noprofs == 0) {
		info("cmd_read_profile: dump_hash_(self/many) ret 0 profs.\n");
	}
	
	/* Copy that data into the provided buffer in a collapsed form.		*/
	copy_size = copy_collapsed(buf, pprofiles, noprofs, KTAU_PROFILE_HDRSIZE);

	if( !(nopids == 1 && pidlist == NULL) ) {
		/* i.e its not self (for evrything else pprofiles is allocated)  */
		free_profile_data(pprofiles, noprofs);
	}

	/* Place the proc-header  info into pquery (later get written to uland)	* 
	 * pquery->bitflag and other fields are already fine. 	*
	 * The size field needs to be filled in */
	info("cmd_read_profile: Exit: profile data size:%lu\n", copy_size);
	pquery->op.read.size = copy_size;
	
	return 0;
}


/* cmd_merge_profile:	Services user merge-op for profile data	*
 *								*
 * [in]	pquery:		ptr to user query			*
 * [in] pidlist:	IGNORE (only for self now)		*
 * [in] nopids:		IGNORE					*
 * [inout] buf:		IGNORE					*
 * [in] buf_size:	IGNORE					*
 *								*
 * Return:	0 on success. Otherwise error			*
 *--------------------------------------------------------------*/
static int cmd_merge_profile(ktau_query* pquery, pid_t* pidlist, int nopids, char* buf, unsigned long buf_size) {
	
	info("cmd_merge_profile: Enter: pquery->op.merge.ppstate:%p\n", pquery->op.merge.ppstate);

	if(pquery->target != KTAU_TARGET_SELF) {
		err("cmd_merge_profile: Wrong target:%d,  Can Only merge 'self'.\n", pquery->target);
		return -ENOTTY;
	}

	return set_hash_mergestate(current->ktau, pquery->op.merge.ppstate);
}


#ifdef CONFIG_KTAU_TRACE
/* cmd_resize_trace:	Performs resize of trace buffer.	*
 * [in]	pquery:		ptr to user query			*
 * [in] pidlist:	list of pids				*
 * [in] nopids:		no pids in pidlist			*
 * [inout] buf:		IGNORE					*
 * [in] buf_size:	IGNORE					*
 * Return:	0 on success. Otherwise error			*
 *--------------------------------------------------------------*/
static int cmd_resize_trace(ktau_query* pquery, pid_t* pidlist, int nopids, char* buf, unsigned long buf_size) {
	int ret = 0;

	info("cmd_resize_trace: Enter: nopids: %d\n", nopids);

	if(pidlist == NULL && nopids == 1) {
		/* then this is 'self' */
		ret = trace_buf_resize_self(current, current->ktau, pquery->op.resize.size);
	} else {
		ret = trace_buf_resize_many(pidlist,  nopids, pquery->op.resize.size);
	}
	
	//set the return value of the query
	pquery->status = ret;

	if(ret < 0) {
		err("cmd_resize_trace: trace_buf_resize_[self/many]() ret < 0 [%x]\n", ret);
		return -1;
	}

	info("cmd_resize_trace: Total Trace ReSize: %lu - No of Processes (0 if self):%d\n", pquery->op.resize.size, ret);

	return 0;
}
#endif /* CONFIG_KTAU_TRACE */

/* cmd_addshcont_profile: Services user shared-container adding *
 *								*
 * [in]	pquery:		ptr to user query			*
 * [in] pidlist:	IGNORE (only for self now)		*
 * [in] nopids:		IGNORE					*
 * [inout] buf:		IGNORE					*
 * [in] buf_size:	IGNORE					*
 *								*
 * Return:	0 on success. Otherwise error			*
 *--------------------------------------------------------------*/
static int cmd_addshcont_profile(ktau_query* pquery, pid_t* pidlist, int nopids, char* buf, unsigned long buf_size) {
	ktau_shcont* shcont = NULL;
	pquery->status = -ENOTTY; //init to fail

	info("cmd_addshcont_profile: Enter: pquery->op.share.ushcont.buf:%p pquery->op.share.ushcont.size:%lu\n", pquery->op.share.ushcont.buf, pquery->op.share.ushcont.size);

	if(pquery->target != KTAU_TARGET_SELF) {
		err("cmd_addshcont_profile: Wrong target:%d,  Can Only merge 'self'.\n", pquery->target);
		return -ENOTTY;
	}

	if((shcont = ktau_share_cont(current, &(pquery->op.share.ushcont))) == NULL) {
		err("cmd_addshcont_profile: ktau_share_cont ret NULL.\n");
		return -ENOTTY;
	}

	//write back info to pquery
	pquery->op.share.ushcont.kid = shcont->kid;
	pquery->status = 0; //SUCCESS

	info("cmd_addshcont_profile: Exit.\n");

	return 0;
}


/* cmd_delshcont_profile: Services user shared-container removal*
 *								*
 * [in]	pquery:		ptr to user query			*
 * [in] pidlist:	IGNORE (only for self now)		*
 * [in] nopids:		IGNORE					*
 * [inout] buf:		IGNORE					*
 * [in] buf_size:	IGNORE					*
 *								*
 * Return:	0 on success. Otherwise error			*
 *--------------------------------------------------------------*/
static int cmd_delshcont_profile(ktau_query* pquery, pid_t* pidlist, int nopids, char* buf, unsigned long buf_size) {
	pquery->status = -ENOTTY; //init to fail

	info("cmd_delshcont_profile: Enter: kid:%d pquery->op.share.ushcont.buf:%p pquery->op.share.ushcont.size:%lu\n", pquery->op.share.ushcont.kid, pquery->op.share.ushcont.buf, pquery->op.share.ushcont.size);
	
	if(pquery->target != KTAU_TARGET_SELF) {
		err("cmd_delshcont_profile: Wrong target:%d,  Can Only merge 'self'.\n", pquery->target);
		return -ENOTTY;
	}

	if((pquery->status = ktau_unshare_cont(current, &(pquery->op.share.ushcont))) != 0) {
		err("cmd_addshcont_profile: ktau_share_cont ret NON-Zero:%d\n", pquery->status);
		return -ENOTTY;
	}

	//nothing else to write-back
	info("cmd_delshcont_profile: Exit.\n");

	return 0;
}


/* cmd_addshctr_profile: Services user shared-counter adding    *
 *								*
 * [in]	pquery:		ptr to user query			*
 * [in] pidlist:	IGNORE (only for self now)		*
 * [in] nopids:		IGNORE					*
 * [inout] buf:		IGNORE					*
 * [in] buf_size:	IGNORE					*
 *								*
 * Return:	0 on success. Otherwise error			*
 *--------------------------------------------------------------*/
static int cmd_addshctr_profile(ktau_query* pquery, pid_t* pidlist, int nopids, char* buf, unsigned long buf_size) {
	int ret = 0;
	pquery->status = -ENOTTY; //init to fail

	info("cmd_addshctr_profile: Enter: pquery->op.ctr.cont_id:%d pquery->op.ctr.ush_ctr.desc.no_syms:%d\n", pquery->op.ctr.cont_id, pquery->op.ctr.ush_ctr.desc.no_syms);

	if(pquery->target != KTAU_TARGET_SELF) {
		err("cmd_addshctr_profile: Wrong target:%d,  Can Only merge 'self'.\n", pquery->target);
		return -ENOTTY;
	}

	if((ret = ktau_ctr_create(current, &(pquery->op.ctr.ush_ctr), pquery->op.ctr.cont_id)) != 0) {
		err("cmd_addshctr_profile: ktau_ctr_create ret Non-Zero:%d.", ret);
		return -ENOTTY;
	}

	//ktau_ctr_create already writes to ush_ctr - so no write back req.
	pquery->status = 0; //SUCCESS

	info("cmd_addshctr_profile: Exit.\n");

	return 0;
}


/* cmd_delshctr_profile: Services user shared-counter removal   *
 *								*
 * [in]	pquery:		ptr to user query			*
 * [in] pidlist:	IGNORE (only for self now)		*
 * [in] nopids:		IGNORE					*
 * [inout] buf:		IGNORE					*
 * [in] buf_size:	IGNORE					*
 *								*
 * Return:	0 on success. Otherwise error			*
 *--------------------------------------------------------------*/
static int cmd_delshctr_profile(ktau_query* pquery, pid_t* pidlist, int nopids, char* buf, unsigned long buf_size) {
	int ret = 0;
	pquery->status = -ENOTTY; //init to fail

	info("cmd_delshctr_profile: Enter: pquery->op.ctr.ush_ctr.desc.kid:%lu\n", pquery->op.ctr.ush_ctr.desc.kid);

	if(pquery->target != KTAU_TARGET_SELF) {
		err("cmd_delshctr_profile: Wrong target:%d,  Can Only merge 'self'.\n", pquery->target);
		return -ENOTTY;
	}

	if((ret = ktau_ctr_delete(current, &(pquery->op.ctr.ush_ctr))) != 0) {
		err("cmd_delshctr_profile: ktau_ctr_delete ret Non-Zero:%d.", ret);
		return -ENOTTY;
	}

	//ktau_ctr_delete already writes to ush_ctr - so no write back req.
	pquery->status = 0; //SUCCESS

	info("cmd_delshctr_profile: Exit.\n");

	return 0;
}

/* cmd_inject_profile: Sets up noise-injection                  *
 *								*
 * [in]	pquery:		ptr to user query			*
 * [in] pidlist:	IGNORE (only for self now)		*
 * [in] nopids:		IGNORE					*
 * [inout] buf:		IGNORE					*
 * [in] buf_size:	IGNORE					*
 *								*
 * Return:	0 on success. Otherwise error			*
 *--------------------------------------------------------------*/
static int cmd_inject_profile(ktau_query* pquery, pid_t* pidlist, int nopids, char* buf, unsigned long buf_size) {
	int ret = 0;
	pquery->status = -ENOTTY; //init to fail

	info("cmd_inject_profile: Enter:\n");

	//1st set the values
	if((ret = ktau_inject_vals(&(pquery->op.inject.val[0]), pquery->op.inject.num))) {
		err("cmd_inject_profile: ktau_inject_vals ret Non-Zero:%d.", ret);
		return -ENOTTY;
	}

	//then send IPI
	if((ret = ktau_inject_IPI())) {
		err("cmd_inject_profile: ktau_inject_IPI ret Non-Zero:%d.", ret);
		return -ENOTTY;
	}

	//ktau_ctr_delete already writes to ush_ctr - so no write back req.
	pquery->status = 0; //SUCCESS

	info("cmd_inject_profile: Exit.\n");

	return 0;
}



/* cmd_noop:	Services user opers that are unimplemented.	*
 *								*
 * [in]	pquery:		IGNORE					*
 * [in] pidlist:	IGNORE 					*
 * [in] nopids:		IGNORE					*
 * [inout] buf:		IGNORE					*
 * [in] buf_size:	IGNORE					*
 *								*
 * Return:	0 						*
 *--------------------------------------------------------------*/
static int cmd_noop(ktau_query* pquery, pid_t* pidlist, int nopids, char* buf, unsigned long buf_size) {

	info("cmd_noop: Unimplemented Operation requested by user-land (pid: %d).\n", current->pid);

	//return -ENOTTY;
	return 0;
}


/* Helpers *
 *---------*/

int calc_profile_size(int nopids, int noentries) {

	unsigned long size = KTAU_PROFILE_HDRSIZE;

	size += sizeof(unsigned int);                                   //<for no-of-profiles>
	size += (nopids * (sizeof(pid_t) + sizeof(unsigned int)));    //<for pid>, no of entries
	size += (noentries * sizeof(o_ent));                      	//space for the entries

	return size;
}


#ifdef CONFIG_KTAU_TRACE
int calc_trace_size(int nopids, unsigned long ktau_trace_size) {

	unsigned long size = KTAU_PROFILE_HDRSIZE;

	size += sizeof(unsigned int);                                   //<for no-of-profiles>
	size += (nopids * (sizeof(pid_t) + sizeof(unsigned int)));	//<for pid>, no of entries
	size += ktau_trace_size;					//space for the entries

	return size;
}
#endif /* CONFIG_KTAU_TRACE */ 


/* validate_query:				*
 * [in] pquery:		ptr to ktau_query 	*
 * Return:	bool - 0 PASS, else fail	*
 *----------------------------------------------*/
int validate_query(ktau_query* pquery) {

	/* validate the type */
	if(!(KTAU_GET_TYPE(pquery->bitflag) >= 0 && KTAU_GET_TYPE(pquery->bitflag) < KTAU_MAX_TYPE)) {
		err("validate_query: bad type: %x\n", KTAU_GET_TYPE(pquery->bitflag));
		return -ENOTTY;
	}

	/* todo: not yet validating the sub-type */

	/* validate the cmd */
	if(!(KTAU_GET_CMD(pquery->bitflag) >= 0 && KTAU_GET_CMD(pquery->bitflag) < KTAU_MAX_CMD)) {
		err("validate_query: bad cmd: %x\n", KTAU_GET_CMD(pquery->bitflag));
		return -ENOTTY;
	}

	/* validate targets */
	/*
	if(!(pquery->target >= 0 && pquery->target < KTAU_MAX_TARGET)) {
		err("validate_query: bad target: %d\n", pquery->target);
		return -ENOTTY;
	}
	*/
	
	/* validate NULLs in cmd-func table */
	if(ktau_user_cmds[KTAU_GET_CMD(pquery->bitflag)][KTAU_GET_TYPE(pquery->bitflag)] == 0) {
		err("validate_query: ktau_user_cmds[cmd][type] is NULL: cmd: %x  type: %x\n", KTAU_GET_CMD(pquery->bitflag), KTAU_GET_TYPE(pquery->bitflag));
		return -ENOTTY;
	}

	info("validate_query: query validaion passed.\n");
	
	return 0;
}

	
/* get_usrpidlist: gets the list of pids from user-space. *
 *                       				  *
 * [inout] ppidlist:		pid_t** 		  *
 *				will be allocated here    *
 *				must deallocate in caller *
 * [in] upidlist:		user-space ptr to pidlist *
 * [in] nopids:			user-space said nopids	  *
 *							  *
 * Returns:			0 Success, non-zero fail  *
 *--------------------------------------------------------*/
static int get_usrpidlist(pid_t** ppidlist, pid_t* upidlist, int nopids) {
	
	int not_copied = 0;
	
	info("get_usrpidlist: &pidlist: %p  nopids:%d  user-pidlist:%p\n", ppidlist, nopids, upidlist);

	*ppidlist = (pid_t*)kmalloc(sizeof(pid_t)*nopids, GFP_KERNEL);
	if(!*ppidlist)
	{
		err("get_usrpidlist: kmalloc failed.\n");
		return -ENOMEM;
	}

	not_copied = copy_from_user(*ppidlist, upidlist, sizeof(pid_t)*nopids);
	if(not_copied != 0) {
		err("get_usrpidlist: copy_from_user failed. not_copied:%d\n", not_copied);
		kfree(*ppidlist);
		return -1;
	}

	return 0;
}


/* put_usrpidlist: puts/frees the list of pids from user-space. *
 *                       				        *
 * [in] pidlist:		pid_t* 		  		*
 *				mem allocated thro get_usrpidlst*
 *								*
 *--------------------------------------------------------------*/
static void put_usrpidlist(pid_t* pidlist) {
	kfree(pidlist);
}


/* 
 * We need a special function that knows how to  
 * free the data that was allocated by dump_hash
 */
static void free_profile_data(ktau_output* profile_data, int noprofs)
{
	int i = 0;

	for(i = 0; i< noprofs; i++)
	{
		kfree(profile_data[i].ent_lst); //<for o_ent*>
	}
}


#ifdef CONFIG_KTAU_TRACE
/* 
 * We need a special function that knows how to  
 * free the trace data that was allocated by dump_trace
 */
static void free_trace_data(ktau_output* profile_data, int noprofs)
{
	int i = 0;

	for(i = 0; i< noprofs; i++)
	{
		vfree(profile_data[i].trace_lst); //<for ktau_trace*>
	}
}
#endif /* CONFIG_KTAU_TRACE */

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

#ifdef CONFIG_KTAU_TRACE
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
#endif /* CONFIG_KTAU_TRACE */

//========================================================
//========================================================
#if 0 /* OLD CODE */
int ktau_ioctl_proc(struct inode *inode,
			  struct file *file, 
			  unsigned int cmd, 
			  unsigned long arg)
{                               
	/* Local variables to accept the user-space in-args */
	ktau_size_t ktau_size;
	ktau_read_t ktau_read;
	ktau_write_t ktau_write;
	ktau_merge_t ktau_merge;
	ktau_package_t hdr;
	unsigned int hdr_size = sizeof(hdr);
	char* user_buf = (char*)arg;
	unsigned int hdrcmd = 0;

	char* temp_buffer = NULL;
	int ret_value = 0;
	unsigned int in_size = 0;
	unsigned int out_size = 0;
	unsigned int not_copied = 0;
	pid_t pid = -1;
	pid_t* pidlist = NULL;

	info("ktau_ioctl_proc: %d . Cmd: %d\n", (current)->pid, cmd);


	not_copied = copy_from_user((char*)&hdr, user_buf, hdr_size);
	if(not_copied != 0) 
	{
		err("ktau_ioctl_proc: copy_form_user(hdr) fail. uncopied : %u\n", not_copied);
		return -1;
	}

	hdrcmd = hdr.type;

	//switch(cmd)
	switch(hdrcmd)
	{
	case CMD_SET_MERGE: /* Set the user-space pointer to long, used for merging profiles : ONLY for SINGLE PID*/
		info("ktau_ioctl_proc: CMD_SET_MERGE: ioctl cmd: %u  hdr.type:%u\n", cmd, hdr.type);
		ktau_merge = hdr.op.merge;
		pid = hdr.pid;
		/* ONLY Self Allwoed to read Single PID : pid can be -1 or self-pid*/
		if((pid != -1) && (pid != current->pid))
		{
			err("ktau_proc.c:: ktau_ioctl_proc: CMD_SET_MERGE: pid invalid: %d\n", pid);
			ret_value = -1;
			goto out;
		}

		ret_value = do_ktau_merge(current, cmd, &ktau_merge, 1, hdr_size);
		if(ret_value < 0)
		{
			err("ktau_proc.c:: ktau_ioctl_proc: CMD_SET_MERGE: do_ktau_merge failed.\n");
			goto out;
		}

		//info("ktau_ioctl_proc: Provided current->ktau->ppstate: %u\n", current->ktau->ppstate);
		info("ktau_ioctl_proc: Provided u-space pointer: %u\n", ktau_merge.ppstate);

		ret_value = 0;
		break;

	case CMD_SIZE: /* Get the size for SINGLE pid */
		info("ktau_ioctl_proc: CMD_SIZE: ioctl cmd: %u  hdr.type:%u\n", cmd, hdr.type);
		ktau_size = hdr.op.size;
		pid = hdr.pid;
		/* ONLY Self Allwoed to read Single PID : pid can be -1 or self-pid*/
		if((pid != -1) && (pid != current->pid))
		{
			ret_value = -1;
			goto out;
		}
		ret_value = do_ktau_size(current, NULL, 0, cmd, &ktau_size, 1, hdr_size);
		if(ret_value < 0)
		{
			goto out;
		}

		info("ktau_ioctl_proc: Tot.Size Req.: %lu\n Size W/O Hdr: %lu", 
				ktau_size.size, ktau_size.size - hdr_size);

		/* We return Size NOT_Including Header-Size */
		ktau_size.size -= hdr_size;
		hdr.op.size = ktau_size;
		copy_to_user(user_buf, (char*)&hdr, hdr_size);
		ret_value = hdr_size;
		break;


#ifdef CONFIG_KTAU_TRACE
	case CMD_TRACE_SIZE: /* Get the size of Trace for SINGLE pid */
#ifdef CONFIG_KTAU_BOOTOPT
		if(!ktau_verify_bootopt(KTAU_TRACE_MSK)) {
			ret_value = -1;
			goto out;
		}
#endif /* CONFIG_KTAU_BOOTOPT*/
		info("ktau_ioctl_proc: CMD_TRACE_SIZE: ioctl cmd: %u  hdr.type:%u\n", cmd, hdr.type);
		ktau_size = hdr.op.size;
		pid = hdr.pid;
		/* ONLY Self Allwoed to read Single PID : pid can be -1 or self-pid*/
		if((pid != -1) && (pid != current->pid))
		{
			ret_value = -1;
			goto out;
		}
		ret_value = do_ktau_trace_size(current, NULL, 1, cmd, &ktau_size, 1, hdr_size);
		if(ret_value < 0)
		{
			goto out;
		}

		info("ktau_ioctl_proc: Tot.Size Req.: %lu\n Size W/O Hdr: %lu", 
				ktau_size.size, ktau_size.size - hdr_size);

		/* We return Size NOT_Including Header-Size */
		ktau_size.size -= hdr_size;
		hdr.op.size = ktau_size;
		copy_to_user(user_buf, (char*)&hdr, hdr_size);
		ret_value = hdr_size;
		break;
#endif /*CONFIG_KTAU_TRACE */


	case CMD_ALL_SIZE: /* Get the size for aLL pid */
		info("ktau_ioctl_proc: CMD_ALL_SIZE: ioctl cmd: %u  hdr.type:%u  hdr.nopids:%d\n"
				, cmd, hdr.type, hdr.nopids);
		ktau_size = hdr.op.size;
		if(hdr.nopids != 0) {
			if(get_usrpidlist(&pidlist, hdr.nopids, user_buf) != 0) {
				ret_value = -1;
				goto out;
			}
		}

		for(ret_value = hdr.nopids; ret_value > 0; ret_value--) {
			info("ktau_ioctl_proc: CMD_ALL_SIZE: pid:%d]. %d\n", ret_value, pidlist[ret_value-1]);
		}

		ret_value = do_ktau_size(NULL, pidlist, hdr.nopids, cmd, &ktau_size, 0, hdr_size);

		free_usrpidlist(&pidlist);

		if(ret_value < 0)
		{
			goto out;
		}

		info("ktau_ioctl_proc: Tot.Size Req.: %lu\n Size W/O Hdr: %lu", 
				ktau_size.size, ktau_size.size - hdr_size);

		/* We return Size NOT_Including Header-Size */
		ktau_size.size -= hdr_size;
		hdr.op.size = ktau_size;
		copy_to_user(user_buf, (char*)&hdr, hdr_size);
		ret_value = hdr_size;
		break;

#ifdef CONFIG_KTAU_TRACE
	case CMD_ALL_TRACE_SIZE: /* Get the size for aLL pid */
#ifdef CONFIG_KTAU_BOOTOPT
		if(!ktau_verify_bootopt(KTAU_TRACE_MSK)) {
			ret_value = -1;
			goto out;
		}
#endif /* CONFIG_KTAU_BOOTOPT*/
		info("ktau_ioctl_proc: CMD_ALL_TRACE_SIZE: ioctl cmd: %u  hdr.type:%u  hdr.nopids:%d\n", 
				cmd, hdr.type, hdr.nopids);
		ktau_size = hdr.op.size;
		if(hdr.nopids != 0) {
			if(get_usrpidlist(&pidlist, hdr.nopids, user_buf) != 0) {
				ret_value = -1;
				goto out;
			}
		}

		for(ret_value = hdr.nopids; ret_value > 0; ret_value--) {
			info("ktau_ioctl_proc: CMD_ALL_SIZE: pid:%d]. %d\n", ret_value, pidlist[ret_value-1]);
		}

		ret_value = do_ktau_trace_size(NULL, pidlist, hdr.nopids, cmd, &ktau_size, 0, hdr_size);
		if(ret_value < 0)
		{
			goto out;
		}

		info("ktau_ioctl_proc: Tot.Size Req.: %lu\n Size W/O Hdr: %lu", 
				ktau_size.size, ktau_size.size - hdr_size);

		/* We return Size NOT_Including Header-Size */
		ktau_size.size -= hdr_size;
		hdr.op.size = ktau_size;
		copy_to_user(user_buf, (char*)&hdr, hdr_size);
		ret_value = hdr_size;
		break;
#endif /*CONFIG_KTAU_TRACE */

	case CMD_READ: /* Read SELF pid */
		info("ktau_ioctl_proc: CMD_READ: ioctl cmd: %u  hdr.type:%u\n", cmd, hdr.type);
		ktau_read = hdr.op.read;
		/* Assume in_size is NOT Including Header-space */
		in_size = ktau_read.size;
		pid = hdr.pid;
		/* ONLY Self Allwoed to read Single PID : pid can be -1 or self-pid*/
		if((pid != -1) && (pid != current->pid))
		{
			ret_value = -1;
			goto out;
		}

		//info("ktau_ioctl_proc: in_size: %lu\n", in_size);

		ret_value = do_ktau_read(current, NULL, 1, cmd, &ktau_read, 1, hdr_size);
		if(ret_value < 0)
		{
			goto out;
		}

		//info("ktau_ioctl_proc: out_size: %lu\n out_size W/O Header: %lu\n", ktau_read.size, ktau_read.size - hdr_size);
 
		/* Keep a ref to data, and set data to NULL so that its    * 
		 * not a junk value in user-space address-space. Then copy *
		 * header. then copy the entire profile data in temp_buf.  *
		 * Free temp_buffer.  */
		temp_buffer = ktau_read.data;
		ktau_read.data = NULL;
		ktau_read.size -= hdr_size;//decr the header-size
		hdr.op.read = ktau_read;

		out_size = ktau_read.size;
		if(out_size > in_size)
		{
			/* Cannot proceed. Cannot return partial binary *
			 * data */
			err("ktau_ioctl_proc: in_size(%u) < out_size(%u). \n",in_size, out_size);
			PROFILE_FREE(temp_buffer);
			hdr.op.read.size = 0;
			not_copied = copy_to_user((char*)arg, &hdr, hdr_size);
			return -1;
		}
		/* Copy the header into the extra-space at the front. *
		 * This extra space is provided by alloc_and_collapse *
		 */
		memcpy(temp_buffer, (char*)&hdr, hdr_size);
		not_copied = copy_to_user(user_buf, temp_buffer, hdr_size + out_size);
		ret_value = out_size + hdr_size;
		if(not_copied != 0) 
		{
			err("ktau_ioctl_proc: copy_to_user(temp_buffer) failed. uncopied : %u\n", not_copied);
			ret_value = -1;
		}

		PROFILE_FREE(temp_buffer);
		break;

#ifdef CONFIG_KTAU_TRACE

	case CMD_TRACE_READ: /* Read SELF pid */
#ifdef CONFIG_KTAU_BOOTOPT
		if(!ktau_verify_bootopt(KTAU_TRACE_MSK)) {
			ret_value = -1;
			goto out;
		}
#endif /* CONFIG_KTAU_BOOTOPT*/
		info("ktau_ioctl_proc: CMD_TRACE_READ: ioctl cmd: %u  hdr.type:%u\n", cmd, hdr.type);
		ktau_read = hdr.op.read;
		/* Assume in_size is NOT Including Header-space */
		in_size = ktau_read.size;
		pid = hdr.pid;
		/* ONLY Self Allwoed to read Single PID : pid can be -1 or self-pid*/
		if((pid != -1) && (pid != current->pid))
		{
			ret_value = -1;
			goto out;
		}

		//info("ktau_ioctl_proc: in_size: %lu\n", in_size);

		ret_value = do_ktau_trace_read(current, NULL, 1, cmd, &ktau_read, 1, hdr_size);
		if(ret_value < 0)
		{
			goto out;
		}

		//info("ktau_ioctl_proc: out_size: %lu\n out_size W/O Header: %lu\n", ktau_read.size, ktau_read.size - hdr_size);
 
		/* Keep a ref to data, and set data to NULL so that its    * 
		 * not a junk value in user-space address-space. Then copy *
		 * header. then copy the entire profile data in temp_buf.  *
		 * Free temp_buffer.  */
		temp_buffer = ktau_read.data;
		ktau_read.data = NULL;
		ktau_read.size -= hdr_size;//decr the header-size
		hdr.op.read = ktau_read;

		out_size = ktau_read.size;
		if(out_size > in_size)
		{
			/* Cannot proceed. Cannot return partial binary *
			 * data */
			err("ktau_ioctl_proc: in_size(%u) < out_size(%u). \n",in_size, out_size);
			vfree(temp_buffer);
			hdr.op.read.size = 0;
			not_copied = copy_to_user((char*)arg, &hdr, hdr_size);
			return -1;
		}
		/* Copy the header into the extra-space at the front. *
		 * This extra space is provided by alloc_and_collapse *
		 */
		info("ktau_ioctl_proc: user_buf: %x  temp_buffer: %x  hdr_size: %u  out_size: %u\n", 
				(unsigned int)user_buf, 
				(unsigned int)temp_buffer, 
				(unsigned int)hdr_size, 
				(unsigned int)out_size);
		memcpy(temp_buffer, (char*)&hdr, hdr_size);
		not_copied = copy_to_user(user_buf, temp_buffer, hdr_size + out_size);
		ret_value = out_size + hdr_size;
		if(not_copied != 0) 
		{
			err("ktau_ioctl_proc: copy_to_user(temp_buffer) failed. uncopied : %u\n", not_copied);
			ret_value = -1;
		}

		vfree(temp_buffer);
		break;
#endif /*CONFIG_KTAU_TRACE */

	case CMD_ALL_READ: /* Read ALL pid */
		info("ktau_ioctl_proc: CMD_ALL_READ: ioctl cmd: %u  hdr.type:%u\n", cmd, hdr.type);
		ktau_read = hdr.op.read;
		/* Assume in_size is NOT Including Header-space */
		in_size = ktau_read.size;

		//info("ktau_ioctl_proc: in_size: %lu\n", in_size);
		if(hdr.nopids != 0) {
			if(get_usrpidlist(&pidlist, hdr.nopids, user_buf) != 0) {
				ret_value = -1;
				goto out;
			}
		}

		ret_value = do_ktau_read(NULL, pidlist, hdr.nopids, cmd, &ktau_read, 0, hdr_size);

		free_usrpidlist(&pidlist);

		if(ret_value < 0)
		{
			goto out;
		}

		//info("ktau_ioctl_proc: out_size: %lu\n out_size W/O Header: %lu\n", ktau_read.size, ktau_read.size - hdr_size);
 
		/* Keep a ref to data, and set data to NULL so that its    * 
		 * not a junk value in user-space address-space. Then copy *
		 * header. then copy the entire profile data in temp_buf.  *
		 * Free temp_buffer.  */
		temp_buffer = ktau_read.data;
		ktau_read.data = NULL;
		ktau_read.size -= hdr_size;//decr the header-size
		hdr.op.read = ktau_read;

		out_size = ktau_read.size;
		if(out_size > in_size)
		{
			/* Cannot proceed. Cannot return partial binary *
			 * data */
			err("ktau_ioctl_proc: in_size(%u) < out_size(%u). \n",in_size, out_size);
			PROFILE_FREE(temp_buffer);
			hdr.op.read.size = 0;
			not_copied = copy_to_user((char*)arg, &hdr, hdr_size);
			return -1;
		}
		/* Copy the header into the extra-space at the front. *
		 * This extra space is provided by alloc_and_collapse *
		 */
		memcpy(temp_buffer, (char*)&hdr, hdr_size);
		not_copied = copy_to_user(user_buf, temp_buffer, hdr_size + out_size);
		ret_value = out_size + hdr_size;
		if(not_copied != 0) 
		{
			err("ktau_ioctl_proc: copy_to_user(temp_buffer) failed. uncopied : %u\n", not_copied);
			ret_value = -1;
		}

		PROFILE_FREE(temp_buffer);
		break;

#ifdef CONFIG_KTAU_TRACE

	case CMD_ALL_TRACE_READ: /* Read ALL TRACE pid */
#ifdef CONFIG_KTAU_BOOTOPT
		if(!ktau_verify_bootopt(KTAU_TRACE_MSK)) {
			ret_value = -1;
			goto out;
		}
#endif /* CONFIG_KTAU_BOOTOPT*/
		info("ktau_ioctl_proc: CMD_ALL_TRACE_READ: ioctl cmd: %u  hdr.type:%u\n", cmd, hdr.type);
		ktau_read = hdr.op.read;
		/* Assume in_size is NOT Including Header-space */
		in_size = ktau_read.size;

		//info("ktau_ioctl_proc: in_size: %lu\n", in_size);
		if(hdr.nopids != 0) {
			if(get_usrpidlist(&pidlist, hdr.nopids, user_buf) != 0) {
				ret_value = -1;
				goto out;
			}
		}

		ret_value = do_ktau_trace_read(NULL, pidlist, hdr.nopids, cmd, &ktau_read, 0, hdr_size);

		free_usrpidlist(&pidlist);

		if(ret_value < 0)
		{
			goto out;
		}

		//info("ktau_ioctl_proc: out_size: %lu\n out_size W/O Header: %lu\n", ktau_read.size, ktau_read.size - hdr_size);
 
		/* Keep a ref to data, and set data to NULL so that its    * 
		 * not a junk value in user-space address-space. Then copy *
		 * header. then copy the entire profile data in temp_buf.  *
		 * Free temp_buffer.  */
		temp_buffer = ktau_read.data;
		ktau_read.data = NULL;
		ktau_read.size -= hdr_size;//decr the header-size
		hdr.op.read = ktau_read;

		out_size = ktau_read.size;
		if(out_size > in_size)
		{
			/* Cannot proceed. Cannot return partial binary *
			 * data */
			err("ktau_ioctl_proc: in_size(%u) < out_size(%u). \n",in_size, out_size);
			vfree(temp_buffer);
			hdr.op.read.size = 0;
			not_copied = copy_to_user((char*)arg, &hdr, hdr_size);
			return -1;
		}
		/* Copy the header into the extra-space at the front. *
		 * This extra space is provided by alloc_and_collapse *
		 */
		info("ktau_ioctl_proc: user_buf: %x  temp_buffer: %x  hdr_size: %u  out_size: %u\n", 
				(unsigned int)user_buf, 
				(unsigned int)temp_buffer, 
				(unsigned int)hdr_size, 
				(unsigned int)out_size);
		memcpy(temp_buffer, (char*)&hdr, hdr_size);
		not_copied = copy_to_user(user_buf, temp_buffer, hdr_size + out_size);
		ret_value = out_size + hdr_size;
		if(not_copied != 0) 
		{
			err("ktau_ioctl_proc: copy_to_user(temp_buffer) failed. uncopied : %u\n", not_copied);
			ret_value = -1;
		}

		vfree(temp_buffer);
		break;
#endif /*CONFIG_KTAU_TRACE */

	case CMD_PURGE: /* Purge SELF pid */
		info("ktau_ioctl_proc: CMD_PURGE: ioctl cmd: %u  hdr.type:%u\n", cmd, hdr.type);
		pid = hdr.pid;
		/* ONLY Self Allwoed to read Single PID : pid can be -1 or self-pid*/
		if((pid != -1) && (pid != current->pid))
		{
			ret_value = -1;
			goto out;
		}

		ktau_write = hdr.op.write;
		ret_value = do_ktau_write(current, cmd, &ktau_write, 1, hdr_size);
		break;


	case CMD_ALL_PURGE: /* Purge all pid */
		info("ktau_ioctl_proc: CMD_ALL_PURGE: ioctl cmd: %u  hdr.type:%u\n", cmd, hdr.type);
		ktau_write = hdr.op.write;
		ret_value = do_ktau_write(NULL, cmd, &ktau_write, 0, hdr_size);
		break;

#ifdef CONFIG_KTAU_TRACE
	case CMD_TRACE_RESIZE: /* Re-size of Trace for SINGLE pid */
#ifdef CONFIG_KTAU_BOOTOPT
		if(!ktau_verify_bootopt(KTAU_TRACE_MSK)) {
			ret_value = -1;
			goto out;
		}
#endif /* CONFIG_KTAU_BOOTOPT*/
		info("ktau_ioctl_proc: CMD_TRACE_RESIZE: ioctl cmd: %u  hdr.type:%u\n", cmd, hdr.type);
		ktau_size = hdr.op.size;
		pid = hdr.pid;
		/* ONLY Self Allwoed to read Single PID : pid can be -1 or self-pid*/
		if((pid != -1) && (pid != current->pid))
		{
			ret_value = -1;
			goto out;
		}
		ret_value = do_ktau_trace_resize(current, NULL, 1, cmd, &ktau_size, 1, hdr_size);
		if(ret_value < 0)
		{
			goto out;
		}

		info("ktau_ioctl_proc: Trc-Resize: Tot.Size Req.: %lu\n Size W/O Hdr: %lu", 
				ktau_size.size, ktau_size.size - hdr_size);

		/* We return Size NOT_Including Header-Size */
		//ktau_size.size -= hdr_size;
		hdr.op.size = ktau_size;
		copy_to_user(user_buf, (char*)&hdr, hdr_size);
		ret_value = hdr_size;
		break;

	case CMD_ALL_TRACE_RESIZE: /* Get the size for aLL pid */
#ifdef CONFIG_KTAU_BOOTOPT
		if(!ktau_verify_bootopt(KTAU_TRACE_MSK)) {
			ret_value = -1;
			goto out;
		}
#endif /* CONFIG_KTAU_BOOTOPT*/
		info("ktau_ioctl_proc: CMD_ALL_TRACE_RESIZE: ioctl cmd: %u  hdr.type:%u  hdr.nopids:%d\n", 
				cmd, hdr.type, hdr.nopids);
		ktau_size = hdr.op.size;
		if(hdr.nopids != 0) {
			if(get_usrpidlist(&pidlist, hdr.nopids, user_buf) != 0) {
				ret_value = -1;
				goto out;
			}
		}

		for(ret_value = hdr.nopids; ret_value > 0; ret_value--) {
			info("ktau_ioctl_proc: CMD_ALL_RESIZE: pid:%d]. %d\n", ret_value, pidlist[ret_value-1]);
		}

		ret_value = do_ktau_trace_resize(NULL, pidlist, hdr.nopids, cmd, &ktau_size, 0, hdr_size);
		if(ret_value < 0)
		{
			goto out;
		}

		info("ktau_ioctl_proc: Tot.ReSize Req.: %lu\n Size W/O Hdr: %lu", 
				ktau_size.size, ktau_size.size - hdr_size);

		/* We return Size NOT_Including Header-Size */
		//ktau_size.size -= hdr_size;
		hdr.op.size = ktau_size;
		copy_to_user(user_buf, (char*)&hdr, hdr_size);
		ret_value = hdr_size;
		break;
#endif /*CONFIG_KTAU_TRACE */

	default: /* Unknow IOCTL Command */
		err("ktau_ioctl_proc: Unknown or Illegal CMD to IOCTL. By <%d>. \n", (current)->pid);
		ret_value = -1;
		goto out;
	}

out:
        return ret_value;
}


/* 
 * ktau_proc helpers:
 * 	Used to collapse the dump_hash output into 
 * 	a single contiguos array of bytes, which   
 * 	can be passed to user-space.
 * 	Returns: number of bytes 
 */
int alloc_and_collapse(char** buf, ktau_output* data, unsigned int noprofs, unsigned int hdr_size)
{
	int alloc_size = 0;
	int i = 0;
	char *bufptr = NULL;

	if(hdr_size < 0)
		hdr_size = 0;

	//info("alloc_and_collapse: req. hdr_size: %u\n", hdr_size);

	alloc_size = hdr_size;

	alloc_size += sizeof(unsigned int);	//<for no-of-profiles>
	for(i = 0; i< noprofs; i++)
	{
		alloc_size += sizeof(pid_t);		//<for pid>
		alloc_size += sizeof(unsigned int);	//<for no-of-entries>
		alloc_size += (sizeof(o_ent) * data[i].size); //<for every o_ent>
	}

	*buf = PROFILE_ALLOC(alloc_size);
	if(!*buf)
	{
		err("alloc_and_collapse: PROFILE_ALLOC failed. alloc_size = %d\n",alloc_size);
		return -1;
	}

	bufptr = *buf + hdr_size; //start from 'after hdr_size'

	memcpy(bufptr, &noprofs, sizeof(unsigned int)); //noprofs
	info("alloc_and_collapse: No Profiles = %u\n",*((unsigned int*)bufptr));
	bufptr += (sizeof(unsigned int));
	for(i = 0; i< noprofs; i++)
	{
		memcpy(bufptr, &(data[i].pid), sizeof(pid_t));//pid
		info("alloc_and_collapse:\tPID = %d",*((pid_t*)bufptr));
		bufptr += (sizeof(pid_t));
		memcpy(bufptr, &(data[i].size), sizeof(unsigned int));//size
		info(", size = %u\n",*((unsigned int*)bufptr));
		bufptr += (sizeof(unsigned int));
		memcpy(bufptr, data[i].ent_lst, sizeof(o_ent) * data[i].size);
		bufptr += (sizeof(o_ent)*data[i].size);
	}



	return alloc_size;
}

#ifdef CONFIG_KTAU_TRACE
/* alloc_and_collapse_trace:
 * 	Used to collapse the dump_trace output into 
 * 	a single contiguos array of bytes, which   
 * 	can be passed to user-space.
 * 	Returns: number of bytes 
 */
int alloc_and_collapse_trace(char** buf, ktau_output* data, unsigned int noprofs, unsigned int hdr_size)
{
	int alloc_size = 0;
	int i = 0;
	char *bufptr = NULL;

	if(hdr_size < 0)
		hdr_size = 0;

	//info("alloc_and_collapse: req. hdr_size: %u\n", hdr_size);

	alloc_size = hdr_size;

	alloc_size += sizeof(unsigned int);	//<for no-of-traces>
	for(i = 0; i< noprofs; i++)
	{
		alloc_size += sizeof(pid_t);		//<for pid>
		alloc_size += sizeof(unsigned int);	//<for no-of-entries>
		alloc_size += (sizeof(ktau_trace) * data[i].size); //<for every ktau_trace>
	}

	*buf = vmalloc(alloc_size);
	if(!*buf)
	{
		err("alloc_and_collapse_trace: vmalloc failed. \n");
		return -1;
	}

	bufptr = *buf + hdr_size; //start from 'after hdr_size'

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
	
	return alloc_size;
}

#endif /*CONFIG_KTAU_TRACE */

/* 
 * We need a special function that knows how to  
 * free the data that was allocated by dump_hash
 */
static void free_profile_data(ktau_output* profile_data, int noprofs)
{
	int i = 0;

	for(i = 0; i< noprofs; i++)
	{
		kfree(profile_data[i].ent_lst); //<for o_ent*>
	}
}


#ifdef CONFIG_KTAU_TRACE
/* 
 * We need a special function that knows how to  
 * free the trace data that was allocated by dump_trace
 */
static void free_trace_data(ktau_output* profile_data, int noprofs)
{
	int i = 0;

	for(i = 0; i< noprofs; i++)
	{
		vfree(profile_data[i].trace_lst); //<for ktau_trace*>
	}
}

#endif /*CONFIG_KTAU_TRACE */

/* Helpers */

static int do_ktau_merge(void* data, unsigned int cmd, ktau_merge_t *ktau_merge, int self, unsigned int hdr_size)
{
	int ret_value = -1;
	struct task_struct* task = (struct task_struct*)data;

	if((task) && (self))
	{
		ret_value = set_hash_mergestate(task->ktau, (ktau_state*) ktau_merge->ppstate);
	}

	//info("do_ktau_merge: ret from set_hash_mergeable ppstate is: %u\n", task->ktau->ppstate);

	return ret_value;
}


static int do_ktau_size(void* data, pid_t* pidlist, int nopids, unsigned int cmd, ktau_size_t *ktau_size, int self, unsigned int hdr_size)
{
	ktau_read_t dummy;
	int ret_value = 0;
	int noent_from_ktau;
	int num_pids = 0;
	struct task_struct* task = (struct task_struct*)data;
	int size = 0;

	dummy.data = NULL;
	dummy.size = -1;
	ktau_size->size = -1;

	if(task) {
		num_pids = 1;
		noent_from_ktau = get_profile_size_self(task->ktau);
	} else {
		num_pids = nopids;
		noent_from_ktau = get_profile_size_many(pidlist, &num_pids);
	}

	size = hdr_size;
	size += sizeof(unsigned int);     				//<for no-of-profiles>
        size += (num_pids * (sizeof(pid_t) + sizeof(unsigned int)));    //<for pid>, no of entries
	size += (noent_from_ktau * sizeof(o_ent));			//space for the entries

	ktau_size->size = size;

	/*ret_value = _do_ktau_read(data, pidlist, nopids, cmd, &dummy, self, hdr_size);

	ktau_size->size = dummy.size;

	//info("do_ktau_size: ret_value from _do_ktau_read: %d , Size: %lu\n", ret_value, ktau_size->size);

	if(dummy.data)
	{
		kfree(dummy.data);
	}
	*/

	return ret_value;
}


static int do_ktau_read(void* data, pid_t* pidlist, int nopids, unsigned int cmd, ktau_read_t *ktau_read, int self, unsigned int hdr_size)
{
	return _do_ktau_read(data, pidlist, nopids, cmd, ktau_read, self, hdr_size);
}

static int do_ktau_write(void* data, unsigned int cmd, ktau_write_t *ktau_write, int self, unsigned int hdr_size)
{
	int ret_value = 0;
	struct task_struct* task = (struct task_struct*)data;

	if(task)
	{
		if(self)
		{
			ret_value = purge_hash_self(task->ktau, 0);
		}
		else
		{
			;//ret_value = purge_hash_other(task->ktau, 0);
		}
	}
	else
	{
		ret_value = purge_hash_many(NULL, 0, 0); //tolerance needs to be taken care of
	}

	return ret_value;
}


/* used by both do_ktau_size & do_ktau_read */
/* if 'self' is 'true', then process asking for own profile
 * otherwise for another processes profile (although still single)*/
static int _do_ktau_read(void* data, pid_t* pidlist, int nopids, unsigned int cmd, ktau_read_t *ktau_read, int self, unsigned int hdr_size)
{
	int ret_value = 0;
	int noprofs = 0;
	ktau_output profile_data;
	ktau_output* pprofile_data = NULL;
	struct task_struct* task = (struct task_struct*)data;
	int alloc_size = 0;
	char* buffer = NULL;

	if(task)
	{
		//HACK: init the profile_data (should it be done in ktau_hash.c?)
		profile_data.pid = task->pid;

		if(self)
		{
			noprofs = dump_hash_self(task, task->ktau, &profile_data, 0);
		}
		else
		{
			;//noprofs = dump_hash_other(task->ktau, &profile_data, 0);
		}

		if(noprofs < 0)
		{
			err("_do_ktau_read: dump_hash failed: noprofs:%d \n", noprofs);
			ret_value = noprofs;
			goto out;
		}

		/* HACK: This needs to be fixed in ktau_hash.c -> return value is wrong */
		noprofs = 1;

		/* then collapse into a single buffer */
		alloc_size = alloc_and_collapse(&buffer, &profile_data, noprofs, hdr_size);

		//info("_do_ktau_read_proc: alloc_size:%d \n", alloc_size);

		/* now free the profile_data as we already copied it */
		free_profile_data(&profile_data, noprofs);
	}
	else
	{
		noprofs = dump_hash_many(pidlist, nopids, &pprofile_data, 0); //tolerance needs to be taken care of

		if(noprofs < 0)
		{
			err("ktau_read_proc: dump_hash failed. \n");
			ret_value = noprofs;
			goto out;
		}

		info("_do_ktau_read: nopids: %d , noprofs(ret): %d\n", nopids, noprofs);


		/* then collapse into a single buffer */
		alloc_size = alloc_and_collapse(&buffer, pprofile_data, noprofs, hdr_size);
		/* now free the profile_data as we already copied it */
		free_profile_data(pprofile_data, noprofs);
		kfree(pprofile_data);
	}


	ktau_read->size = alloc_size;
	ktau_read->data = buffer;
out:		
	return ret_value;
}

#ifdef CONFIG_KTAU_TRACE
static int do_ktau_trace_size(void* data, pid_t* pidlist, int nopids, unsigned int cmd, ktau_size_t *ktau_size, int self, unsigned int hdr_size)
{
	ktau_read_t dummy;
	int ret_value = 0;
	struct task_struct* task = (struct task_struct*)data;
	int size_from_ktau = 0;
	int num_pids = nopids;

	dummy.data = NULL;
	dummy.size = -1;
	ktau_size->size = -1;

	if(task) { //self
		get_trace_size_self(task, task->ktau, &size_from_ktau);
	} else { //many?
		if(get_trace_size_many(pidlist, nopids, &size_from_ktau) <= 0) {
			err("do_ktau_trace_size: get_trace_size_many ret <= 0\n");
			return -1;
		}
	}

	size_from_ktau += hdr_size;						//header
	size_from_ktau += sizeof(unsigned int);					//no of profs
	size_from_ktau += (num_pids * (sizeof(pid_t) + sizeof(unsigned int)));	//pid + no-of-trace-entries
	
	ktau_size->size = size_from_ktau;

	/*
	ret_value = _do_ktau_trace_read(data, pidlist, nopids, cmd, &dummy, self, hdr_size);

	ktau_size->size = dummy.size;

	//info("do_ktau_size: ret_value from _do_ktau_read: %d , Size: %lu\n", ret_value, ktau_size->size);

	if(dummy.data)
	{
		vfree(dummy.data);
	}
	*/

	return ret_value;
}


static int do_ktau_trace_read(void* data, pid_t* pidlist, int nopids, unsigned int cmd, ktau_read_t *ktau_read, int self, unsigned int hdr_size)
{
	return _do_ktau_trace_read(data, pidlist, nopids, cmd, ktau_read, self, hdr_size);
}


/* used by both do_ktau_trace_size & do_ktau_trace_read */
/* if 'self' is 'true', then process asking for own trace
 * otherwise for another processes trace (although still single)*/
static int _do_ktau_trace_read(void* data, pid_t* pidlist, int nopids, unsigned int cmd, ktau_read_t *ktau_read, int self, unsigned int hdr_size)
{
	int ret_value = 0;
	int noprofs = 0;
	ktau_output profile_data;
	ktau_output* pprofile_data = NULL;
	struct task_struct* task = (struct task_struct*)data;
	int alloc_size = 0;
	char* buffer = NULL;

	if(task)
	{
		//HACK: init the profile_data (should it be done in ktau_hash.c?)
		profile_data.pid = task->pid;

		if(self)
		{
			noprofs = dump_trace_self(task->ktau, &profile_data);
		}
		else
		{
			;//noprofs = dump_hash_other(task->ktau, &profile_data, 0);
		}

		if(noprofs < 0)
		{
			err("_do_ktau_trace_read: dump_trace failed: noprofs:%d \n", noprofs);
			ret_value = noprofs;
			goto out;
		}

		/* HACK: This needs to be fixed in ktau_hash.c -> return value is wrong */
		noprofs = 1;

		/* then collapse into a single buffer */
		//alloc_size = alloc_and_collapse(&buffer, &profile_data, noprofs, hdr_size);
		alloc_size = alloc_and_collapse_trace(&buffer, &profile_data, noprofs, hdr_size);

		//info("_do_ktau_read_proc: alloc_size:%d \n", alloc_size);

		/* now free the profile_data as we already copied it */
		free_trace_data(&profile_data, noprofs);
	}
	else
	{
		noprofs = dump_trace_many(pidlist, nopids, &pprofile_data); //no tolerance here?

		if(noprofs < 0)
		{
			err("ktau_read_trace_proc: dump_trace failed. \n");
			ret_value = noprofs;
			goto out;
		}

		/* then collapse into a single buffer */
		alloc_size = alloc_and_collapse_trace(&buffer, pprofile_data, noprofs, hdr_size);
		/* now free the profile_data as we already copied it */
		free_trace_data(pprofile_data, noprofs);
		kfree(pprofile_data);
	}


	ktau_read->size = alloc_size;
	ktau_read->data = buffer;
out:		
	return ret_value;
}

static int do_ktau_trace_resize(void* data, pid_t* pidlist, int nopids, unsigned int cmd, ktau_size_t *ktau_size, int self, unsigned int hdr_size)
{
	int ret_value = 0;
	struct task_struct* task = (struct task_struct*)data;

	if(task) { //self
		trace_buf_resize_self(task, task->ktau, ktau_size->size);
	} else { //many?
		if(trace_buf_resize_many(pidlist,  nopids, ktau_size->size)<=0) {
			err("do_ktau_trace_resize: get_trace_size_many ret <= 0\n");
			return -1;
		}
	}

	return ret_value;
}

#endif /*CONFIG_KTAU_TRACE */


static int get_usrpidlist(pid_t** ppidlist, int nopids, char* user_buf) {
	
	int not_copied = -1;
	pid_t* pidlist = NULL;
	
	info("get_usr_pidlist: &pidlist: %u  nopids:%d\n", ppidlist, nopids);

	pidlist = (pid_t*)kmalloc(sizeof(pid_t)*nopids, GFP_KERNEL);

	if(!pidlist)
	{
		err("get_usr_pidlist: kmalloc failed.\n");
		return -1;
	}

	not_copied = copy_from_user(pidlist, user_buf+sizeof(ktau_package_t), sizeof(pid_t)*nopids);
	if(not_copied != 0) {
		err("get_usr_pidlist: copy_from_user failed.\n");
		kfree(pidlist);
		return -1;
	}
	
	*ppidlist = pidlist;

	return 0;
}

static void free_usrpidlist(pid_t** ppidlist) {
	kfree(*ppidlist);
}

#endif /* 0 - OLD CODE */

//==========================================
//==========================================

