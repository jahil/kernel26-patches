/**************************************************************************
 * File		: include/linux/ktau/ktau_proc_int.h
 * Version 	: $Id: ktau_proc_int.h,v 1.5.2.4 2007/04/12 04:18:23 anataraj Exp $
 *************************************************************************/
#ifndef _KTAU_PROC_INT_H
#define _KTAU_PROC_INT_H

#include <linux/types.h>

#include <linux/ktau/ktau_proc_data.h>

#define TAU_PROC_ROOT "ktau"
#define TAU_PROC_MAX_NAME 256

#define KTAU_PROFILE_HDRSIZE (sizeof(ktau_query))

/* /proc/ktau internal kernel stuff *
 *----------------------------------*/


/* KTAU_PROC_CMD function type *
 *-----------------------------*/
typedef int (*CMD_FUNCPTR)(ktau_query* pquery, pid_t* pidlist, int nopids, char* buf, unsigned long buf_size);


/* fops for /proc/ktau/... *
 *-------------------------*/
extern struct file_operations tau_fops;


/* IOCTL FUNCTION *
 *----------------*/
int ktau_ioctl_proc(struct inode *inode,
			  struct file *file, 
			  unsigned int cmd, 
			  unsigned long arg);


/* User-Cmd servicing functions *
 *------------------------------*/

/* cmd_size_profile:	Performs size calc for profile data.	*
 * [in]	pquery:		ptr to user query			*
 * [in] pidlist:	list of pids				*
 * [in] nopids:		no pids in pidlist			*
 * [inout] buf:		IGNORE					*
 * [in] buf_size:	IGNORE					*
 * Return:	0 on success. Otherwise error			*
 *--------------------------------------------------------------*/
static int cmd_size_profile(ktau_query* pquery, pid_t* pidlist, int nopids, char* buf, unsigned long buf_size);


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
static int cmd_read_profile(ktau_query* pquery, pid_t* pidlist, int nopids, char* buf, unsigned long buf_size);


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
static int cmd_merge_profile(ktau_query* pquery, pid_t* pidlist, int nopids, char* buf, unsigned long buf_size);


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
static int cmd_noop(ktau_query* pquery, pid_t* pidlist, int nopids, char* buf, unsigned long buf_size); 


#ifdef CONFIG_KTAU_TRACE
/* cmd_size_trace:	Performs size calc for trace data.	*
 * [in]	pquery:		ptr to user query			*
 * [in] pidlist:	list of pids				*
 * [in] nopids:		no pids in pidlist			*
 * [inout] buf:		IGNORE					*
 * [in] buf_size:	IGNORE					*
 * Return:	0 on success. Otherwise error			*
 *--------------------------------------------------------------*/
static int cmd_size_trace(ktau_query* pquery, pid_t* pidlist, int nopids, char* buf, unsigned long buf_size);


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
static int cmd_read_trace(ktau_query* pquery, pid_t* pidlist, int nopids, char* buf, unsigned long buf_size);


/* cmd_resize_trace:	Performs resize of trace buffer. 	*
 * [in]	pquery:		ptr to user query			*
 * [in] pidlist:	list of pids				*
 * [in] nopids:		no pids in pidlist			*
 * [inout] buf:		IGNORE					*
 * [in] buf_size:	IGNORE					*
 * Return:	0 on success. Otherwise error			*
 *--------------------------------------------------------------*/
static int cmd_resize_trace(ktau_query* pquery, pid_t* pidlist, int nopids, char* buf, unsigned long buf_size);

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
static int cmd_addshcont_profile(ktau_query* pquery, pid_t* pidlist, int nopids, char* buf, unsigned long buf_size); 


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
static int cmd_delshcont_profile(ktau_query* pquery, pid_t* pidlist, int nopids, char* buf, unsigned long buf_size);

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
static int cmd_addshctr_profile(ktau_query* pquery, pid_t* pidlist, int nopids, char* buf, unsigned long buf_size);

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
static int cmd_delshctr_profile(ktau_query* pquery, pid_t* pidlist, int nopids, char* buf, unsigned long buf_size);


/* cmd_inject_profile: Services user shared-counter removal   *
 *								*
 * [in]	pquery:		ptr to user query			*
 * [in] pidlist:	IGNORE (only for self now)		*
 * [in] nopids:		IGNORE					*
 * [inout] buf:		IGNORE					*
 * [in] buf_size:	IGNORE					*
 *								*
 * Return:	0 on success. Otherwise error			*
 *--------------------------------------------------------------*/
static int cmd_inject_profile(ktau_query* pquery, pid_t* pidlist, int nopids, char* buf, unsigned long buf_size);

/* Helpers *
 *---------*/

static int calc_profile_size(int nopids, int noentries);


/* validate_query:				*
 * [in] pquery:		ptr to ktau_query 	*
 * Return:	bool - 0 PASS, else fail	*
 *----------------------------------------------*/
static int validate_query(ktau_query* pquery);

	
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
static int get_usrpidlist(pid_t** ppidlist, pid_t* upidlist, int nopids);


/* put_usrpidlist: puts/frees the list of pids from user-space. *
 *                       				        *
 * [in] pidlist:		pid_t* 		  		*
 *				mem allocated thro get_usrpidlst*
 *								*
 *--------------------------------------------------------------*/
static void put_usrpidlist(pid_t* pidlist);


/* 
 * We need a special function that knows how to  
 * free the data that was allocated by dump_hash
 * Note: This introduces a tight-coupling between
 * ktau_hash and ktau_proc. This should be in hash.
 */
static void free_profile_data(ktau_output* profile_data, int noprofs);

#ifdef CONFIG_KTAU_TRACE
static int calc_trace_size(int nopids, unsigned long ktau_trace_size); 

/* 
 * We need a special function that knows how to  
 * free the trace data that was allocated by dump_trace
 */
static void free_trace_data(ktau_output* profile_data, int noprofs);
#endif /* CONFIG_KTAU_TRACE */

int copy_collapsed(char* buf, ktau_output* data, unsigned int noprofs, unsigned int hdr_size);

#ifdef CONFIG_KTAU_TRACE
int copy_collapsed_trace(char* buf, ktau_output* data, unsigned int noprofs, unsigned int hdr_size);
#endif /* CONFIG_KTAU_TRACE */

#endif /* _KTAU_PROC_INT_H */
