/*************************************************************
 * File         : $
 * Version      : $Id: ktau_proc_interface.c,v 1.10.2.10 2008/02/26 03:27:55 anataraj Exp $
 *
 ************************************************************/

/* pid_t is needed for compatibility between kern-space	*
 * & user-space. But including linux/types.h causes	*
 * a lot of conflicts. Hence HACK: typedefing		*
 * seperately.						*
 *------------------------------------------------------*/
/* HACK -> ARCH-DEPEND? */
typedef int pid_t;

/* u32 & u8 used in ktau kernel headers *
 * are not defined in user-space. This  *
 * is  a HACK defining them here. 	*
 * THIS IS ARCH-DEPEND and CAN BREAK!!!	*
 *--------------------------------------*/
//typedef unsigned int u32;
//typedef unsigned char u8;

/* ktau user-spc-interface headers */
#include <ktau_proc_interface.h>

/* ktau KERNEL headers */
#include <linux/ktau/ktau_proc_data.h>

/* user-sp headers */
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/* User-Space Application Interface Functions */


/* Read & Purge PIDs 				    *
 * If nopids = 1, then used for single pid          *
 * If nopids > 1, then used for multiple pids       *
 * if nopids = 0 & pid=null, then used for ALL Pids *
 */

#define MAX_KTAU_PROC_PATH 1024

#define PROFILE_PATH "/proc/ktau/profile"
#define TRACE_PATH "/proc/ktau/trace"

/* Helper function to form the query header to send to kernel */
static int ktau_build_query(ktau_query* pquery, int type, int cmd, int self, pid_t *pidlist, unsigned int nopids, unsigned long tol, unsigned int* pflags) {

	/* 1st clear the pquery->bitflag */
	pquery->bitflag = 0;

	/* Set the Info-Type */
	KTAU_SET_TYPE(pquery->bitflag, type);

	/* Set the Info-SubType - unused - set to zero now*/
	KTAU_SET_SUBTYPE(pquery->bitflag, 0);

	/* Set the CMD */
	KTAU_SET_CMD(pquery->bitflag, cmd);

	/* Set the SubCMD - unused - set to zero now*/
	KTAU_SET_SUBCMD(pquery->bitflag, 0);

	/* Set the PIDList & NoPids */
	pquery->pidlist = pidlist;
	pquery->nopids = nopids;

	/* Set the Target */
	if(self) {
		pquery->target = KTAU_TARGET_SELF;
		pquery->pidlist = NULL;
		pquery->nopids = 1;
	//} else if((pidlist == NULL) && (nopids == 0)) {
	} else if(nopids == 0) { //not checking is pidlist==NULL becoz timeKtau fails then.
		pquery->target = KTAU_TARGET_ALL;
	} else if((pidlist != NULL) && (nopids == 1)) {
		pquery->target = KTAU_TARGET_PID;
	} else if((pidlist != NULL) && (nopids > 1)) {
		pquery->target = KTAU_TARGET_MANY;
	} else {
		/* flag error */
		return -1;
	}

	/* Set the flags */
	pquery->flags.mask = 0;
	pquery->flags.tol.tol_in = tol;

	if(pflags)
		pquery->flags.mask = *pflags;

	//printf("ktau_build_query: Query: Type:%x,  SubType:%x,  Cmd:%x,  SubCmd:%x,  Target:%x,  pidlist:%x,  nopids:%d\n", (pquery->bitflag)&0xF000, (pquery->bitflag)&0x0F00, (pquery->bitflag)&0x00F0, (pquery->bitflag)&0x000F, pquery->target, pquery->pidlist, pquery->nopids);
	return 0;
}


/* Helper function to choose /proc/ktau/<file> and open it */
static int ktau_open_proc(int *pfd, ktau_query* pquery) {
	char* path = NULL;

	switch(KTAU_GET_TYPE(pquery->bitflag)) {
		case KTAU_TYPE_PROFILE:
			path = PROFILE_PATH;
			break;
		case KTAU_TYPE_TRACE:
			path = TRACE_PATH;
			break;
		default:
			fprintf(stderr, "ktau_open_proc: unknown type (neither KTAU_TYPE Profile nor Trace): (val) %d\n", KTAU_GET_TYPE(pquery->bitflag));
			break;
	}

	if(!path) {
		return -1;
	}

	*pfd = open(path,O_RDONLY);	
	if(*pfd < 0)
	{
		perror("ktau_open_proc: open");
		fprintf(stderr, "ktau_open_proc: open(%s) failed.\n", path);
		return *pfd;
	}

	return 0;
}


/* Helper function to close /proc/ktau/<file> */
static void ktau_close_proc(int fd) {
	close(fd);
}

/* Helper function to allocate and init a new *
 * ktau_ushcont */
static ktau_ushcont* alloc_ushcont(unsigned long size) {
	unsigned long tot_size = 0;
	char* buf = NULL;
	ktau_ushcont* new_ushcont = NULL;

	/* Size calc: Need to allocate enough memory for: *
	 * 1. ktau_ushcont
	 * 2. ktau_dbl_buf
	 * 3. each of the buffers within the   *
	 * the ktau_dblbuf (of length "size"). */
	tot_size += sizeof(ktau_ushcont);
	tot_size += sizeof(ktau_dbl_buf);
	tot_size += (size*2);
	
	//mem allocation
	buf = calloc(1, tot_size);
	if(!buf) {
		return NULL;
	}
	
	//mem structuring
	new_ushcont = (ktau_ushcont*)buf;
	//point to end of ushcont
	new_ushcont->buf = (ktau_dbl_buf*)(buf + sizeof(ktau_ushcont));
	
	//initialization (calloc does most of it);
	new_ushcont->size = size;

	return new_ushcont;
}

/* Helper function to de-allocate a *
 * ktau_ushcont */
static void dealloc_ushcont(ktau_ushcont* ushcont) {
	if(!ushcont) {
		return;
	}
	//zero it out
	memset(ushcont, 0, ktau_ushcont_size(ushcont));
	free(ushcont);
	return;
}

/* Helpers to setup/remove mem sequence inside shared-cont *
 * for a test during creation inside kernel. */
static void setup_shcont_test(ktau_ushcont* ushcont) {
	int *val1, *val2;
	//val1 = (int*)(ushcont->buf->buf[0]);
	//val2 = (int*)(ushcont->buf->buf[1]);
	val1 = (int*)(ktau_shcont2dblbuf(ushcont, 0));
	val2 = (int*)(ktau_shcont2dblbuf(ushcont, 1));
	*val1 = 0xdeadbeef;
	*val2 = 0xbeefdead;
	return;
}
static void clear_shcont_test(ktau_ushcont* ushcont) {
	int *val1, *val2;
	//val1 = (int*)(ushcont->buf->buf[0]);
	//val2 = (int*)(ushcont->buf->buf[1]);
	val1 = (int*)(ktau_shcont2dblbuf(ushcont, 0));
	val2 = (int*)(ktau_shcont2dblbuf(ushcont, 1));
	*val1 = *val2 = 0x0;
	return;
}
static int check_shcont_k2u(ktau_ushcont* ushcont) {
	int *val1, *val2;
	val1 = (int*)(ktau_shcont2dblbuf(ushcont, 0));
	val2 = (int*)(ktau_shcont2dblbuf(ushcont, 1));
	if((*val1 != 0xbadbeef) || (*val2 != 0xbeefbad)) {;
		return 0;
	}
	return 1;
}

unsigned long long ktau_rdtsc(void);
long ktau_noop_timed(unsigned long long* timings) {
	ktau_query query;
	int ktau_fd = 0, ret_value = 0;
	unsigned long lc_size = 0;
	unsigned long long tmp_time = 0;

	/* initialize ktau_query with user-args */
	if(ktau_build_query(&query, KTAU_TYPE_PROFILE, KTAU_CMD_INJECT+1, 1, NULL, 0, 0, NULL)) {
		return -1;
	}

	/* Get the ktau_proc open file-desc */
	if(timings) tmp_time = ktau_rdtsc();
	if(ktau_open_proc(&ktau_fd, &query)) {
		return -1;
	}
	if(timings) timings[0] += (ktau_rdtsc() - tmp_time);

	/* Make the ioctl into ktau_proc */
	if(timings) tmp_time = ktau_rdtsc();
	ret_value = ioctl(ktau_fd, KTAU_PROC_MAGIC_NOOP, &query);
	if(timings) timings[1] += (ktau_rdtsc() - tmp_time);

	/* close ktau_proc file-des */
	if(timings) tmp_time = ktau_rdtsc();
	ktau_close_proc(ktau_fd);
	if(timings) timings[2] += (ktau_rdtsc() - tmp_time);

	if(ret_value < 0)
	{
		perror("ioctl:");
	}

	return ret_value;
}

/* read_size: Used to query /proc/ktau for SIZE of profile(s). *
 ***************************************************************
 * Arguments:
 * type:        KTAU_TYPE_PROFILE or KTAU_TYPE_TRACE
 *
 * self:        if reading self -> set to 1, otherwise set to zero
 *
 * pid:         pointer to a single pid , or address of array of pids
 *              Can be NULL (if nopids is ZERO) --> size of FULL sys. prof
 *
 * nopids:      no of pids being pointed to above
 *              Can be ZERO --> request size of entire system prof
 *
 * tol:         tolerance paramter. to ignore, set to ZERO
 *
 * flags:       <TO BE DONE> . ignore, set to NULL.
 *
 * compratio:	This is a fraction between 0 & 1 (can be zero or 1 also)
 * 		-1 --> means USE DEFAULT VALUE
 * 		Compensation-Ratio Used to scale-up the size returned 
 * 		from /proc, so that when a call is made to read_data, 
 * 		the size is larger to accomodate any changes to profile. 
 * 		NOTE: To Ignore SET to -1. 
 *
 * Returns:
 * On Success:  total size of profile(s) of pid(s)
 * On Error:    -1; Other negative error codes reserved.
 *
 * Constraints:
 * None.
 */
long read_size(int type, int self, pid_t *pidlist, unsigned int nopids, unsigned long tol, unsigned int* pflags, float compratio) {
	return read_size_timed(type, self, pidlist, nopids, tol, pflags, compratio, NULL);
}

long read_size_timed(int type, int self, pid_t *pidlist, unsigned int nopids, unsigned long tol, unsigned int* pflags, float compratio, unsigned long long* timings)
{
	ktau_query query;
	int ktau_fd = 0, ret_value = 0;
	unsigned long lc_size = 0;
	unsigned long long tmp_time = 0;

	/* initialize ktau_query with user-args */
	if(ktau_build_query(&query, type, KTAU_CMD_SIZE, self, pidlist, nopids, tol, pflags)) {
		return -1;
	}

	/* Get the ktau_proc open file-desc */
	if(timings) tmp_time = ktau_rdtsc();
	if(ktau_open_proc(&ktau_fd, &query)) {
		return -1;
	}
	if(timings) timings[0] += (ktau_rdtsc() - tmp_time);

	/* Make the ioctl into ktau_proc */
	if(timings) tmp_time = ktau_rdtsc();
	ret_value = ioctl(ktau_fd, KTAU_PROC_MAGIC, &query);
	if(timings) timings[1] += (ktau_rdtsc() - tmp_time);

	/* Record the returned size */
	lc_size = query.op.size.size;

	/* Record the returned flags into users flags (if any) */
	if(pflags)
		*pflags = query.flags.mask;

	/* close ktau_proc file-des */
	if(timings) tmp_time = ktau_rdtsc();
	ktau_close_proc(ktau_fd);
	if(timings) timings[2] += (ktau_rdtsc() - tmp_time);

	if(ret_value < 0)
	{
		perror("ioctl:");
		return ret_value;
	}

	ret_value = 0;

	/* We add some compensation to the actual size. *
	 * we do this since the size of profile/trace   *
 	 * can change after this call. Compensation	*
	 * reduces the chance the read-buffer is too 	*
	 * small.					*
	 *----------------------------------------------*/
	if(compratio == -1)
	{	/* By default(i.e: -1) compensation is 50% */
		compratio = 0.5;
	}

	lc_size = lc_size + (unsigned long)(lc_size * compratio);

	/* return compensated size */
	return lc_size;
}


/* read_data: Used to query /proc/ktau for DATA of profile(s). *
 ***************************************************************
 * Arguments:
 * type:        KTAU_TYPE_PROFILE or KTAU_TYPE_TRACE
 *
 * self:        if reading self -> set to 1, otherwise set to zero
 *
 * pidlist:     pointer to a single pid , or address of array of pids
 *              Can be NULL --> request data of entire system prof
 *
 * nopids:      no of pids being pointed to above
 *              Can be ZERO --> request data of entire system prof
 *
 * buffer:      pointer to allocated memory
 *
 * size:        size of buffer (allocate memory above)
 *
 * tol:         tolerance paramter. to ignore, set to ZERO
 *
 * flags:       <TO BE DONE> . ignore, set to NULL.
 *
 * Returns:
 * On Success:  total size of profile(s) data read into buffer.
 * On Error:    -1; Other negative error codes reserved.
 *
 * Constraints:
 * Must have called read_size before to ascertain size of buffer.
 *
 * NOTE: Even if SIZE allocated is that returned by read_size, 
 * read_data can return error for lack of size -> as size of prof
 * can change after read_size. This is unlikely.
 */
long read_data(int type, int self, pid_t *pidlist, unsigned int nopids, char* buffer, unsigned long size, unsigned long tol, unsigned int* pflags) {
	return read_data_timed(type, self, pidlist, nopids, buffer, size, tol, pflags, NULL);
}

long read_data_timed(int type, int self, pid_t *pidlist, unsigned int nopids, char* buffer, unsigned long size, unsigned long tol, unsigned int* pflags, unsigned long long* timings)
{
	ktau_query query;
	int ktau_fd = 0, ret_value = 0;
	unsigned long long tmp_time = 0;

	/* initialize ktau_query with user-args */
	if(ktau_build_query(&query, type, KTAU_CMD_READ, self, pidlist, nopids, tol, pflags)) {
		return -1;
	}
	
	/* Add Cmd-Read specific query data */
	query.op.read.size = size;
	query.op.read.buf = buffer;

	if(timings) tmp_time = ktau_rdtsc();
	/* Get the ktau_proc open file-desc */
	if(ktau_open_proc(&ktau_fd, &query)) {
		return -1;
	}
	if(timings) timings[0] += (ktau_rdtsc() - tmp_time);

	/* Make the ioctl into ktau_proc */
	if(timings) tmp_time = ktau_rdtsc();
	ret_value = ioctl(ktau_fd, KTAU_PROC_MAGIC, &query);
	if(timings) timings[1] += (ktau_rdtsc() - tmp_time);

	/* Record the returned flags into users flags (if any) */
	if(pflags)
		*pflags = query.flags.mask;

	/* close ktau_proc file-des */
	if(timings) tmp_time = ktau_rdtsc();
	ktau_close_proc(ktau_fd);
	if(timings) timings[2] += (ktau_rdtsc() - tmp_time);

	if(ret_value < 0)
	{
		perror("read_data: ioctl ret val < 0:");
		return ret_value;
	}

	/* on success, return the size of the read buffer */
	return query.op.read.size;
}


/* purge_data: Used to reset state of profiles /proc/ktau for pid(s). *
 **********************************************************************
 * Arguments:
 * pid:         pointer to a single pid , or address of array of pids
 *              Can be NULL --> request reset of entire system prof
 *
 * nopids:      no of pids being pointed to above
 *              Can be ZERO --> request reset of entire system prof
 *
 * tol:         tolerance paramter. to ignore, set to ZERO
 *
 * flags:       <TO BE DONE> . ignore, set to NULL.
 *
 * Returns:
 * On Success:  0
 * On Error:    -1; Other negative error codes reserved.
 *
 * Constraints:
 * None.
 * ======================= NOT USED CURRENTLY ======================== *
 *---------------------------------------------------------------------*/

/*------------------------------------------------------------------------------------------------*
long purge_data(pid_t *pid, unsigned int nopids, unsigned long tol, unsigned int* flags)
{
	int ret_value = 0;
	int fd = -1;
	char path[MAX_KTAU_PROC_PATH+1];
	ktau_package_t ktau_pkg;
	ktau_write_t kbuffer;
	char* lc_buf = NULL;
	char* temp_buf = NULL;

	//initialize buffer with user-args
	kbuffer.tol.tol_in = tol;
	kbuffer.flags = 0;
	if(flags)
		kbuffer.flags = *flags;

	lc_buf = (char*)malloc(sizeof(ktau_package_t));
	if(!lc_buf)
	{
		perror("malloc:");
		return -1;
	}

	if(nopids == 1) // single pid write : use the write method 
	{
		// CHANGED: TO use only all & ioctl 
		//  snprintf(path, MAX_KTAU_PROC_PATH, "/proc/ktau/%u", *pid);
		//
		snprintf(path, MAX_KTAU_PROC_PATH, "/proc/ktau/all");

		fd = open(path,O_RDWR);	
		if(fd < 0)
		{
			perror("open:");
			free(lc_buf);
			return fd;
		}

		ktau_pkg.op.write = kbuffer; // so that flags/tolerance passed from app is recorded 
		ktau_pkg.type = CMD_PURGE;
		ktau_pkg.pid = *pid;
		
		memcpy(lc_buf, &ktau_pkg, sizeof(ktau_pkg));

		ret_value = 0;
		// CHANGED: To use ioctl ONLY
		//  do {
		//	ret_value = write(fd, lc_buf, sizeof(ktau_package_t));
		//} while((ret_value < 0) && (errno == EINTR));
		//
		ret_value = ioctl(fd, CMD_PURGE, lc_buf);

		ktau_pkg = *((ktau_package_t*)lc_buf);
	}
	else if(nopids == 0) // ALL Pids : use IOCTL 
	{
		snprintf(path, MAX_KTAU_PROC_PATH, "/proc/ktau/all");

		fd = open(path,O_RDWR);	
		if(fd < 0)
		{
			perror("open:");
			free(lc_buf);
			return fd;
		}

		ktau_pkg.op.write = kbuffer; // So that flags/ tolerance passed from app is recorded 
		ktau_pkg.type = CMD_ALL_PURGE;
		
		memcpy(lc_buf, &ktau_pkg, sizeof(ktau_pkg));

		ret_value = ioctl(fd, CMD_ALL_PURGE, lc_buf);

		ktau_pkg = *((ktau_package_t*)lc_buf);
	}
	else //Many pids : not supported 
	{
		perror("pruge_data: Many PIDs not supported. Only Single & ALL Pids.");
		ret_value = -1;
	}

	close(fd);

	if(ret_value < 0)
	{
		perror("write / ioctl:");
		if(lc_buf)
		{
			free(lc_buf);
		}
		return ret_value;
	}

	kbuffer = ktau_pkg.op.write;

	if(flags)
		*flags = kbuffer.flags;

	ret_value = 0;
	
	free(lc_buf);

	return ret_value;
}
*------------------------------------------------------------------------------------------------*/

/* ktau_set_state : Used to query /proc/ktau for SIZE of profile(s). *
 ***************************************************************
 * Arguments:
 * pid:         pointer to a single pid (MUST BE SELF)
 *              Can be NULL (if pid is unknown, e.g. for threads) 
 *
 * state:	ptr to ktau_state (CAN be NULL, to Unset)
 *
 * flags:       <TO BE DONE> . ignore, set to NULL.
 *
 * Returns:
 * On Success:  Zero.
 * On Error:    -1; Other negative error codes reserved.
 *
 * Constraints:
 * ktau_set_state only works on 'SELF' , cannot perform it (yet) on 
 * other processes.
 */
long ktau_set_state(pid_t *pid, ktau_state* state, unsigned int* pflags)
{
	int ktau_fd = 0, ret_value = 0;
	ktau_query query;

	/* initialize ktau_query with user-args */
	/* Currently only do PROFILE merging, no Trace merging */
	/* Also note: for self, pid is ignored in ktau_build_query */
	if(ktau_build_query(&query, KTAU_TYPE_PROFILE, KTAU_CMD_MERGE, 1, pid, 1, 0, pflags)) {
		return -1;
	}
	
	/* Add Cmd-Merge specific query data */
	query.op.merge.ppstate = state;

	/* Get the ktau_proc open file-desc */
	if(ktau_open_proc(&ktau_fd, &query)) {
		return -1;
	}

	/* Make the ioctl into ktau_proc */
	ret_value = ioctl(ktau_fd, KTAU_PROC_MAGIC, &query);

	/* Record the returned flags into users flags (if any) */
	if(pflags)
		*pflags = query.flags.mask;

	/* close ktau_proc file-des */
	ktau_close_proc(ktau_fd);

	if(ret_value < 0)
	{
		perror("ktau_set_state: ioctl ret val < 0:");
		return ret_value;
	}

	/* on success, return 0 */
	return 0;
}

/* ktau_trace_resize: Used to resize trace-buffer of kernel *
 ***************************************************************
 * Arguments:
 * newsize:	no of entries to resize into (not memory size)
 *
 * type:        KTAU_TYPE_PROFILE or KTAU_TYPE_TRACE
 *
 * self:        if reading self -> set to 1, otherwise set to zero
 *
 * pid:         pointer to a single pid , or address of array of pids
 *              Can be NULL (if nopids is ZERO) --> size of FULL sys. prof
 *
 * nopids:      no of pids being pointed to above
 *              Can be ZERO --> request size of entire system prof
 *
 * tol:         tolerance paramter. to ignore, set to ZERO
 *
 * pflags:       <TO BE DONE> . ignore, set to NULL.
 *
 * Returns:
 * On Success:  total size of profile(s) of pid(s)
 * On Error:    -1; Other negative error codes reserved.
 *
 * Constraints:
 * None.
 */
long ktau_trace_resize(int newsize, int type, int self, pid_t *pidlist, unsigned int nopids, unsigned long tol, unsigned int* pflags)
{
	int ktau_fd = 0, ret_value = 0;
	ktau_query query;

	/* initialize ktau_query with user-args */
	/* Currently only do Trace resizing */
	/* Also note: for self, pid is ignored in ktau_build_query */
	if(ktau_build_query(&query, KTAU_TYPE_TRACE, KTAU_CMD_RESIZE, self, pidlist, nopids, tol, pflags)) {
		return -1;
	}
	
	/* Add Cmd-Resize specific query data */
	query.op.resize.size = newsize;

	/* Get the ktau_proc open file-desc */
	if(ktau_open_proc(&ktau_fd, &query)) {
		return -1;
	}

	/* Make the ioctl into ktau_proc */
	ret_value = ioctl(ktau_fd, KTAU_PROC_MAGIC, &query);

	/* Record the returned flags into users flags (if any) */
	if(pflags)
		*pflags = query.flags.mask;

	/* close ktau_proc file-des */
	ktau_close_proc(ktau_fd);

	if(ret_value < 0)
	{
		perror("ktau_trace_resize: ioctl ret val < 0:");
		return ret_value;
	}
	
	//TODO: we must interpret the query.status field
	//and report errors...

	/* on success, just return newsize */
	return query.op.resize.size;
}

/* ktau_add_container: Used to create a shared container (a memory  *
 * buffer that is shared between user/kernel and that usually  *
 * holds a counter (which exports some performance metric).    *
 ***************************************************************
 * Arguments:
 * type:	KTAU_PROFILE or KTAU_TRACE
 *		(Currently only KTAU_PROFILE accepted)
 *
 * self:        if reading self -> set to 1, otherwise set to zero
 *		NOTE: This call only accepts "self=1", else fails.
 *
 * pid:		IGNORED: pointer to a single pid , or address of array of pids
 *		(Redundant for this call - as only self accepted).
 *
 * nopids:	IGNORED: no of pids being pointed to above
 * 		Can be ZERO --> request size of entire system prof
 *		(Redundant for this call - as only self accepted).
 *
 * pflags:	<TO BE DONE> . ignore, set to NULL.
 *
 * size:	Size of required buffer, must be > 0.
 *
 * Returns:
 * On Success:	ptr to new ktau_ushcont.
 * On Error:	NULL.
 *
 * Constraints:
 * - Works only in 'self' mode (i.e cannot request shared-cont 
 * in other process(es)).
 * - The obtained ktau_ushcont* must be "freed" using 
 * ktau_del_container.
 */
extern ktau_ushcont* ktau_add_container(int type, int self, pid_t *pid, unsigned int nopids, unsigned int* pflags, unsigned long size) {
	int ktau_fd = 0, ret_value = 0;
	ktau_query query;
	ktau_ushcont* new_ushcont = NULL;

	/* initialize ktau_query with user-args */
	/* Currently only do PROFILE merging, no Trace merging */
	/* Also note: for self, pid is ignored in ktau_build_query */
	if(ktau_build_query(&query, KTAU_TYPE_PROFILE, KTAU_CMD_ADDSHCONT, 1, NULL, 1, 0, pflags)) {
		return NULL;
	}
	
	/* Alloc and init a new ktau_ushcont and the ktau_dblbuf. *
	 * Currently we do as one block. */
	new_ushcont = alloc_ushcont(size); 
	if(!new_ushcont) {
		return NULL;
	}

	/* setup Test */
	setup_shcont_test(new_ushcont);

	/* Add Cmd-Merge specific query data */
	query.op.share.ushcont = *new_ushcont;

	/* Get the ktau_proc open file-desc */
	if(ktau_open_proc(&ktau_fd, &query)) {
		goto free_out;
	}

	/* Make the ioctl into ktau_proc */
	ret_value = ioctl(ktau_fd, KTAU_PROC_MAGIC, &query);

	/* Record the returned flags into users flags (if any) */
	if(pflags)
		*pflags = query.flags.mask;

	/* close ktau_proc file-des */
	ktau_close_proc(ktau_fd);

	//TODO: Interpret query.status

	if(ret_value < 0)
	{
		perror("ktau_add_container: ioctl ret val < 0:");
		goto free_out;
	}

	/* do the k2u test */
	if(!check_shcont_k2u(new_ushcont)) {
		//then what do we do?
		printf("ktau_add_container: check_shcont_k2u failed. deleting container....");
		ktau_del_container(type, self, pid, nopids, pflags, new_ushcont);
		goto free_out;
	}
	/* clear Test */
	clear_shcont_test(new_ushcont);

	/* on success, return new_ushcont */
	return new_ushcont;
free_out:
	dealloc_ushcont(new_ushcont);
	return NULL;
}

/* ktau_del_container: Destroys a shared contaier previously   *
 * created with ktau_add_container.                            *
 ***************************************************************
 * Arguments:
 * type:	KTAU_PROFILE or KTAU_TRACE
 *		(Currently only KTAU_PROFILE accepted)
 *
 * self:        if reading self -> set to 1, otherwise set to zero
 *		NOTE: This call only accepts "self=1", else fails.
 *
 * pid:		IGNORED: pointer to a single pid , or address of array of pids
 *		(Redundant for this call - as only self accepted).
 *
 * nopids:	IGNORED: no of pids being pointed to above
 * 		Can be ZERO --> request size of entire system prof
 *		(Redundant for this call - as only self accepted).
 *
 * pflags:	<TO BE DONE> . ignore, set to NULL.
 *
 * pushcont:	ptr to ktau_ushcont (previously obtained from 
 *		ktau_add_container).
 *
 * Returns:
 * On Success:	0.
 * On Error:	-ve.
 *
 * Constraints:
 * - Works only in 'self' mode (i.e cannot request shared-cont 
 * in other process(es)).
 * - The provided ktau_ushcont* must have been got from using 
 * ktau_add_container.
 */
extern int ktau_del_container(int type, int self, pid_t *pid, unsigned int nopids, unsigned int* pflags, ktau_ushcont* pushcont) {
	int ktau_fd = 0, ret_value = 0;
	ktau_query query;

	if(!pushcont) {
		return -1;
	}

	/* initialize ktau_query with user-args */
	/* Currently only do PROFILE merging, no Trace merging */
	/* Also note: for self, pid is ignored in ktau_build_query */
	if(ktau_build_query(&query, KTAU_TYPE_PROFILE, KTAU_CMD_DELSHCONT, 1, NULL, 1, 0, pflags)) {
		return -1;
	}
	
	/* Add Cmd-Merge specific query data */
	query.op.share.ushcont = *pushcont;

	/* Get the ktau_proc open file-desc */
	if(ktau_open_proc(&ktau_fd, &query)) {
		return -1;
	}

	/* Make the ioctl into ktau_proc */
	ret_value = ioctl(ktau_fd, KTAU_PROC_MAGIC, &query);

	/* Record the returned flags into users flags (if any) */
	if(pflags)
		*pflags = query.flags.mask;

	/* close ktau_proc file-des */
	ktau_close_proc(ktau_fd);

	//TODO: Interpret query.status

	if(ret_value < 0)
	{
		perror("ktau_del_container: ioctl ret val < 0:");
		return ret_value;
	}

	/* Got here - now dealloc this ushcont */
	dealloc_ushcont(pushcont);

	return 0;
}

/* ktau_del_container_user:
 * Same as ktau_del_container, except this only deletes the container in user-space *
 * and does not inform the kernel about this change - in effect assumes that a fork *
 * occurred and that the mem-map in OS of new process does not have shared-mapping. *
 *
 * NOTE: Dont call if this isnt right after a fork. Specifically implemented for    *
 * calling from TAU, not to be used otherwise - effects undefined.                  */
extern int ktau_del_container_user(int type, int self, pid_t *pid, unsigned int nopids, unsigned int* pflags, ktau_ushcont* pushcont) {
	if(!pushcont) {
		return -1;
	}

	dealloc_ushcont(pushcont);
 
	return 0;
}

/* ktau_get_counter: Creates and registers a new shared-ctr    *
 ***************************************************************
 * Arguments:
 * pushcont:	ptr to ktau_ushcont obtained from ktau_add_container
 *
 * name:	name of this counter
 *
 * no_syms:	number of symbols (or points) counter will observe
 *
 * syms:	arr of symbols - must be no_syms long atleast
 *
 * ppctr:	out-arg -- will result in allocated ctr on success.
 *
 * Returns:
 * On Success:	0.
 * On Error:	-ve.
 *
 * Constraints:
 * - Works only in 'self' mode (i.e cannot request shared-ctr 
 * in other process(es)).
 * - The provided ktau_ushcont* must have been got from using 
 * ktau_add_container.
 */
int ktau_get_counter(ktau_ushcont* pushcont, char name[KTAU_MAX_COUNTERNAME], int no_syms, unsigned long* syms, ktau_ush_ctr** ppctr) {
	int ktau_fd = 0, ret_value = 0;
	ktau_query query;

	if(!pushcont || !ppctr) {
		return -1;
	}

	if((no_syms <= 0) || (no_syms > KTAU_MAX_COUNTERSYMS)) {
		return -1;
	}

	/* initialize ktau_query with user-args */
	/* Currently only do PROFILE merging, no Trace merging */
	/* Also note: for self, pid is ignored in ktau_build_query */
	if(ktau_build_query(&query, KTAU_TYPE_PROFILE, KTAU_CMD_ADDSHCTR, 1, NULL, 1, 0, NULL)) {
		return -1;
	}
	
	/* Add cmd specific query data */
	query.op.ctr.cont_id = pushcont->kid;
	//setup ush_ctr & desc
	query.op.ctr.ush_ctr.type = KTAU_COUNTER_SHARED;
	strncpy(query.op.ctr.ush_ctr.desc.name, name, KTAU_MAX_COUNTERNAME-1);
	query.op.ctr.ush_ctr.desc.name[KTAU_MAX_COUNTERNAME-1] = '\0';
	query.op.ctr.ush_ctr.desc.no_syms = no_syms;
	memcpy(query.op.ctr.ush_ctr.desc.syms, syms, sizeof(unsigned long)*no_syms);
	 
	/* Get the ktau_proc open file-desc */
	if(ktau_open_proc(&ktau_fd, &query)) {
		return -1;
	}

	/* Make the ioctl into ktau_proc */
	ret_value = ioctl(ktau_fd, KTAU_PROC_MAGIC, &query);

	/* close ktau_proc file-des */
	ktau_close_proc(ktau_fd);

	//TODO: Interpret query.status

	if(ret_value < 0)
	{
		perror("ktau_get_counter: ioctl ret val < 0:");
		return ret_value;
	}

	/* Last step - if alls well - then alloc ktau_ush_ctr and init it */
	*ppctr = (ktau_ush_ctr*)calloc(sizeof(ktau_ush_ctr),1);
	if(!*ppctr) {
		perror("ktau_get_counter: calloc of ktau_ush_ctr failed.");
		return -1;
	}
	//now init and return
	**ppctr = query.op.ctr.ush_ctr;
	(*ppctr)->ucont = pushcont;

	return ret_value;
}

/* ktau_put_counter: "returns" exisiting counter shared-ctr    *
 ***************************************************************
 * Arguments:
 * pshctr:	ptr to ktau_ush_ctr obtained from ktau_get_counter
 */
int ktau_put_counter(ktau_ush_ctr* pushctr) {
	int ktau_fd = 0, ret_value = 0;
	ktau_query query;

	if(!pushctr) {
		return -1;
	}

	/* initialize ktau_query with user-args */
	/* Currently only do PROFILE merging, no Trace merging */
	/* Also note: for self, pid is ignored in ktau_build_query */
	if(ktau_build_query(&query, KTAU_TYPE_PROFILE, KTAU_CMD_DELSHCTR, 1, NULL, 1, 0, NULL)) {
		return -1;
	}
	
	/* Add cmd specific query data */
	query.op.ctr.ush_ctr = *pushctr;
	query.op.ctr.cont_id = pushctr->ucont->kid;

	/* Get the ktau_proc open file-desc */
	if(ktau_open_proc(&ktau_fd, &query)) {
		return -1;
	}

	/* Make the ioctl into ktau_proc */
	ret_value = ioctl(ktau_fd, KTAU_PROC_MAGIC, &query);

	/* close ktau_proc file-des */
	ktau_close_proc(ktau_fd);

	//TODO: Interpret query.status

	if(ret_value < 0)
	{
		perror("ktau_get_counter: ioctl ret val < 0 (ctr mem not freed):");
		return ret_value;
	}

	//alls well? free mem
	free(pushctr);

	return ret_value;
}

int ktau_put_counter_user(ktau_ush_ctr* pushctr) {
	free(pushctr);
	return 0;
}

#define KTAU_USER_BARRIER() asm __volatile__ ( "" : : : "memory" )

inline static int _ktau_toggle_dblbuf(ktau_dbl_buf* dblbuf) {
	dblbuf->cur_index = 1 - dblbuf->cur_index;//1st toggle it
	return (1 - dblbuf->cur_index); //then return old index
}

/* ktau_copy_counter: Copies from shared-ctr into
 * user-provided buffer of ktau_data[] 
 * Returns 0 on Success. -ve on Error.
 */ 
int ktau_copy_counter(ktau_ush_ctr* pushctr, ktau_data* data, int no_data) {
	int inactive = -1, tmp_no_data = 0;
	ktau_data* ctr_data = NULL;

	if(no_data > pushctr->desc.no_syms || no_data <= 0) {
		return -1;
	}
	
	//toggle cur_index of dbl_buf before reading
	inactive = _ktau_toggle_dblbuf(pushctr->ucont->buf);

	KTAU_USER_BARRIER();

	ctr_data = ktau_shctr2ctrdata(pushctr, inactive);

	tmp_no_data = no_data;
	//1st time just assign
	while(tmp_no_data--) {
		data[tmp_no_data] = ctr_data[tmp_no_data];
	}

//#warning "We are only reading one buffer of the double-buf - ok?"
//	return 0; 

	//again
	KTAU_USER_BARRIER();

	//toggle cur_index of dbl_buf before reading
	inactive = _ktau_toggle_dblbuf(pushctr->ucont->buf);

	KTAU_USER_BARRIER();

	ctr_data = ktau_shctr2ctrdata(pushctr, inactive);

	tmp_no_data = no_data;
	//2nd time add
	while(tmp_no_data--) {
		data[tmp_no_data].timer.incl += ctr_data[tmp_no_data].timer.incl;
		data[tmp_no_data].timer.excl += ctr_data[tmp_no_data].timer.excl;
		data[tmp_no_data].timer.count += ctr_data[tmp_no_data].timer.count;
	}

	return 0; 
}

/* ktau_copy_counter_type: Copies val based on type (num, excl or incl) from shared-ctr into
 * user-provided buffer of unsigned long long
 * Returns 0 on Success. -ve on Error.
 */ 
int ktau_copy_counter_type(ktau_ush_ctr* pushctr, unsigned long long* data, int no_data, int* cType) {
	int inactive = -1, tmp_no_data = 0;
	ktau_data* ctr_data = NULL;

	if(no_data > pushctr->desc.no_syms || no_data <= 0) {
		return -1;
	}
	
	//toggle cur_index of dbl_buf before reading
	inactive = _ktau_toggle_dblbuf(pushctr->ucont->buf);

	KTAU_USER_BARRIER();

	ctr_data = ktau_shctr2ctrdata(pushctr, inactive);

	tmp_no_data = no_data;
	//1st time just assign
	while(tmp_no_data--) {
		if(cType[tmp_no_data] == KTAU_SHCTR_TYPE_EXCL) {
			data[tmp_no_data] = ctr_data[tmp_no_data].timer.excl;
		} else if(cType[tmp_no_data] == KTAU_SHCTR_TYPE_INCL) {
			data[tmp_no_data] = ctr_data[tmp_no_data].timer.incl;
		} else if(cType[tmp_no_data] == KTAU_SHCTR_TYPE_NUM) {
			data[tmp_no_data] = ctr_data[tmp_no_data].timer.count;
		} else {
			data[tmp_no_data] = 0xdeadbeef;
		}
	}

	//again
	KTAU_USER_BARRIER();

	//toggle cur_index of dbl_buf before reading
	inactive = _ktau_toggle_dblbuf(pushctr->ucont->buf);

	KTAU_USER_BARRIER();

	ctr_data = ktau_shctr2ctrdata(pushctr, inactive);

	tmp_no_data = no_data;
	//2nd time add
	while(tmp_no_data--) {
		if(cType[tmp_no_data] == KTAU_SHCTR_TYPE_EXCL) {
			data[tmp_no_data] += ctr_data[tmp_no_data].timer.excl;
		} else if(cType[tmp_no_data] == KTAU_SHCTR_TYPE_INCL) {
			data[tmp_no_data] += ctr_data[tmp_no_data].timer.incl;
		} else if(cType[tmp_no_data] == KTAU_SHCTR_TYPE_NUM) {
			data[tmp_no_data] += ctr_data[tmp_no_data].timer.count;
		} else {
			data[tmp_no_data] = 0xdeadbeef;
		}
	}

	return 0; 
}

/* ktau_copy_counter_excl: Copies exclusive val from shared-ctr into
 * user-provided buffer of unsigned long long
 * Returns 0 on Success. -ve on Error.
 */ 
int ktau_copy_counter_excl(ktau_ush_ctr* pushctr, unsigned long long* data, int no_data) {
	int inactive = -1, tmp_no_data = 0;
	ktau_data* ctr_data = NULL;

	if(no_data > pushctr->desc.no_syms || no_data <= 0) {
		return -1;
	}
	
	//toggle cur_index of dbl_buf before reading
	inactive = _ktau_toggle_dblbuf(pushctr->ucont->buf);

	KTAU_USER_BARRIER();

	ctr_data = ktau_shctr2ctrdata(pushctr, inactive);

	tmp_no_data = no_data;
	//1st time just assign
	while(tmp_no_data--) {
		data[tmp_no_data] = ctr_data[tmp_no_data].timer.excl;
	}

	//again
	KTAU_USER_BARRIER();

	//toggle cur_index of dbl_buf before reading
	inactive = _ktau_toggle_dblbuf(pushctr->ucont->buf);

	KTAU_USER_BARRIER();

	ctr_data = ktau_shctr2ctrdata(pushctr, inactive);

	tmp_no_data = no_data;
	//2nd time add
	while(tmp_no_data--) {
		data[tmp_no_data] += ctr_data[tmp_no_data].timer.excl;
	}

	return 0; 
}

/* ktau_copy_counter_incl: Copies inclusive val from shared-ctr into
 * user-provided buffer of unsigned long long
 * Returns 0 on Success. -ve on Error.
 */ 
int ktau_copy_counter_incl(ktau_ush_ctr* pushctr, unsigned long long* data, int no_data) {
	int inactive = -1, tmp_no_data = 0;
	ktau_data* ctr_data = NULL;

	if(no_data > pushctr->desc.no_syms || no_data <= 0) {
		return -1;
	}
	
	//toggle cur_index of dbl_buf before reading
	inactive = _ktau_toggle_dblbuf(pushctr->ucont->buf);

	KTAU_USER_BARRIER();

	ctr_data = ktau_shctr2ctrdata(pushctr, inactive);

	tmp_no_data = no_data;
	//1st time just assign
	while(tmp_no_data--) {
		data[tmp_no_data] = ctr_data[tmp_no_data].timer.incl;
	}

	//again
	KTAU_USER_BARRIER();

	//toggle cur_index of dbl_buf before reading
	inactive = _ktau_toggle_dblbuf(pushctr->ucont->buf);

	KTAU_USER_BARRIER();

	ctr_data = ktau_shctr2ctrdata(pushctr, inactive);

	tmp_no_data = no_data;
	//2nd time add
	while(tmp_no_data--) {
		data[tmp_no_data] += ctr_data[tmp_no_data].timer.incl;
	}

	return 0; 
}

/* ktau_copy_counter_count: Copies count (no. of times event occured) from shared-ctr into
 * user-provided buffer of unsigned long long
 * Returns 0 on Success. -ve on Error.
 */ 
int ktau_copy_counter_count(ktau_ush_ctr* pushctr, unsigned long long* data, int no_data) {
	int inactive = -1, tmp_no_data = 0;
	ktau_data* ctr_data = NULL;

	if(no_data > pushctr->desc.no_syms || no_data <= 0) {
		return -1;
	}
	
	//toggle cur_index of dbl_buf before reading
	inactive = _ktau_toggle_dblbuf(pushctr->ucont->buf);

	KTAU_USER_BARRIER();

	ctr_data = ktau_shctr2ctrdata(pushctr, inactive);

	tmp_no_data = no_data;
	//1st time just assign
	while(tmp_no_data--) {
		data[tmp_no_data] = ctr_data[tmp_no_data].timer.count;
	}

	//again
	KTAU_USER_BARRIER();

	//toggle cur_index of dbl_buf before reading
	inactive = _ktau_toggle_dblbuf(pushctr->ucont->buf);

	KTAU_USER_BARRIER();

	ctr_data = ktau_shctr2ctrdata(pushctr, inactive);

	tmp_no_data = no_data;
	//2nd time add
	while(tmp_no_data--) {
		data[tmp_no_data] += ctr_data[tmp_no_data].timer.count;
	}

	return 0; 
}


/* ktau_inject_noise: *
 **********************
 * Arguments:
 * unsigned long long* vals
 * int num
 *
 */
int ktau_inject_noise(unsigned long long *vals, int num) {
	int ktau_fd = 0, ret_value = 0;
	ktau_query query;

	if((!vals) || (num != KTAU_MAX_INJECT_VALS)) {
		return -1;
	}

	/* initialize ktau_query with user-args */
	if(ktau_build_query(&query, KTAU_TYPE_PROFILE, KTAU_CMD_INJECT, 1, NULL, 1, 0, NULL)) {
		return -1;
	}
	
	/* Add cmd specific query data */
	int i = 0;
	for(i = 0; i<KTAU_MAX_INJECT_VALS; i++) {
		query.op.inject.val[i] = vals[i];
	}
	query.op.inject.num = num;

	/* Get the ktau_proc open file-desc */
	if(ktau_open_proc(&ktau_fd, &query)) {
		return -1;
	}

	/* Make the ioctl into ktau_proc */
	ret_value = ioctl(ktau_fd, KTAU_PROC_MAGIC, &query);

	/* close ktau_proc file-des */
	ktau_close_proc(ktau_fd);

	//TODO: Interpret query.status

	if(ret_value < 0)
	{
		perror("ktau_inject_noise: ioctl ret val < 0 :");
		return ret_value;
	}

	return ret_value;
}
#if 0 /* OLD CODE  - K1 */
/* read_size: Used to query /proc/ktau for SIZE of profile(s). *
 ***************************************************************
 * Arguments:
 * type:        KTAU_PROFILE or KTAU_TRACE
 *
 * self:        if reading self -> set to 1, otherwise set to zero
 *
 * pid:         pointer to a single pid , or address of array of pids
 *              Can be NULL (if nopids is ZERO) --> size of FULL sys. prof
 *
 * nopids:      no of pids being pointed to above
 *              Can be ZERO --> request size of entire system prof
 *
 * tol:         tolerance paramter. to ignore, set to ZERO
 *
 * flags:       <TO BE DONE> . ignore, set to NULL.
 *
 * compratio:	This is a fraction between 0 & 1 (can be zero or 1 also)
 * 		-1 --> means USE DEFAULT VALUE
 * 		Compensation-Ratio Used to scale-up the size returned 
 * 		from /proc, so that when a call is made to read_data, 
 * 		the size is larger to accomodate any changes to profile. 
 * 		NOTE: To Ignore SET to -1. 
 *
 * Returns:
 * On Success:  total size of profile(s) of pid(s)
 * On Error:    -1; Other negative error codes reserved.
 *
 * Constraints:
 * None.
 */
long read_size(int type, int self, pid_t *pid, unsigned int nopids, unsigned long tol, unsigned int* flags, float compratio)
{
	int ret_value = 0;
	int fd = -1;
	char path[MAX_KTAU_PROC_PATH+1];
	ktau_package_t ktau_pkg;
	ktau_size_t size;
	unsigned long lc_size = 0;
	unsigned int cmd = 0;
	char* hdrbuf = NULL;


	//printf("DEBUG: size of o_ent %d\n",sizeof(o_ent));
	//printf("DEBUG: size of h_ent %d\n",sizeof(h_ent));
	//printf("DEBUG: size of ktau_data %d\n",sizeof(ktau_data));
	//printf("DEBUG: size of ktau_timer %d\n",sizeof(ktau_timer));
	//printf("DEBUG: size of ktau_pcounter %d\n",sizeof(ktau_perf_counter));


	/* initialize ktau_size_t with user-args */
	size.tol.tol_in = tol;
	size.flags = 0;
	if(flags)
		size.flags = *flags;

	switch(type) {
	case KTAU_PROFILE:
		snprintf(path, MAX_KTAU_PROC_PATH, PROFILE_PATH);
		break;
	case KTAU_TRACE:
		snprintf(path, MAX_KTAU_PROC_PATH, TRACE_PATH);
		break;
	default:
		//def is profile
		snprintf(path, MAX_KTAU_PROC_PATH, PROFILE_PATH);
	}


	if((nopids == 1) && (self == 1)) /* single pid get size : use the read method */
	{
		switch(type) {
		case KTAU_PROFILE:
			cmd = CMD_SIZE;
			break;
		case KTAU_TRACE:
			cmd = CMD_TRACE_SIZE;
			break;
		default:
			//def is profile
			cmd = CMD_SIZE;
		}

		//fd = open(path,O_RDWR);	
		fd = open(path,O_RDONLY);	
		if(fd < 0)
		{
			perror("open:");
			return fd;
		}

		ktau_pkg.op.size = size;
		ktau_pkg.type = cmd;
		ktau_pkg.pid = *pid;
		ktau_pkg.nopids = nopids;
		ret_value = 0;
		
		/* CHANGED BELOW: Using ONLY ioctl now. And can only do SELF OR ALL
		 * do {
			ret_value = write(fd, &ktau_pkg, sizeof(ktau_package_t));
		} while((ret_value < 0) && (errno == EINTR));
		*/
		ret_value = ioctl(fd, cmd, &ktau_pkg);

		lc_size = ktau_pkg.op.size.size;
		if(flags)
			*flags = ktau_pkg.op.size.flags;

	}
	else //if (nopids == 0) /* ALL Pids: use IOCTL method */
	{
		switch(type) {
		case KTAU_PROFILE:
			cmd = CMD_ALL_SIZE;
			break;
		case KTAU_TRACE:
			cmd = CMD_ALL_TRACE_SIZE;
			break;
		default:
			//def is profile
			cmd = CMD_ALL_SIZE;
		}

		//fd = open(path,O_RDWR);	
		fd = open(path,O_RDONLY);	
		if(fd < 0)
		{
			perror("open:");
			return fd;
		}

		ktau_pkg.op.size = size;
		ktau_pkg.type = cmd;
		ktau_pkg.pid = -1;
		ktau_pkg.nopids = nopids;
		
		if(nopids == 0) { //then ALL processes
			hdrbuf = (char*)&ktau_pkg;
		} else { //nopids >= 1, then some other or many processes
			hdrbuf = (char*)malloc(sizeof(ktau_package_t) + (sizeof(pid_t) * nopids));
			*((ktau_package_t*)hdrbuf) = ktau_pkg;
			memcpy(hdrbuf+sizeof(ktau_package_t), pid, sizeof(pid_t)*nopids);
		}

		ret_value = 0;
		
		ret_value = ioctl(fd, cmd, hdrbuf);
		
		ktau_pkg = *((ktau_package_t*)hdrbuf);

		lc_size = ktau_pkg.op.size.size;
		if(flags)
			*flags = ktau_pkg.op.size.flags;

	}
	//else /* Many Pids: We dont handle it now */
	//{
	//	perror("read_size: Dont Handle Multiple PIDs now. Only Single & ALL.\n");
	//	ret_value = -1;
	//}

	close(fd);

	if(ret_value < 0)
	{
		perror("read:");
		return ret_value;
	}

	ret_value = 0;

	if(compratio == -1)
	{	
		compratio = 0.5;
	}

	lc_size = lc_size + (unsigned long)(lc_size * compratio);

	return lc_size;
}


/* ktau_trace_resize: Used to resize trace-buffer of kernel *
 ***************************************************************
 * Arguments:
 * newsize:	no of entries to resize into (not memory size)
 *
 * type:        KTAU_PROFILE or KTAU_TRACE
 *
 * self:        if reading self -> set to 1, otherwise set to zero
 *
 * pid:         pointer to a single pid , or address of array of pids
 *              Can be NULL (if nopids is ZERO) --> size of FULL sys. prof
 *
 * nopids:      no of pids being pointed to above
 *              Can be ZERO --> request size of entire system prof
 *
 * tol:         tolerance paramter. to ignore, set to ZERO
 *
 * flags:       <TO BE DONE> . ignore, set to NULL.
 *
 * Returns:
 * On Success:  total size of profile(s) of pid(s)
 * On Error:    -1; Other negative error codes reserved.
 *
 * Constraints:
 * None.
 */
long ktau_trace_resize(int newsize, int type, int self, pid_t *pid, unsigned int nopids, unsigned long tol, unsigned int* flags)
{
	int ret_value = 0;
	int fd = -1;
	char path[MAX_KTAU_PROC_PATH+1];
	ktau_package_t ktau_pkg;
	ktau_size_t size;
	unsigned long lc_size = 0;
	unsigned int cmd = 0;
	char* hdrbuf = NULL;


	//printf("DEBUG: size of o_ent %d\n",sizeof(o_ent));
	//printf("DEBUG: size of h_ent %d\n",sizeof(h_ent));
	//printf("DEBUG: size of ktau_data %d\n",sizeof(ktau_data));
	//printf("DEBUG: size of ktau_timer %d\n",sizeof(ktau_timer));
	//printf("DEBUG: size of ktau_pcounter %d\n",sizeof(ktau_perf_counter));


	/* initialize ktau_size_t with user-args */
	size.tol.tol_in = tol;
	size.flags = 0;
	if(flags)
		size.flags = *flags;

	//Set the Requested size - actually number of entries
	size.size = newsize;

	switch(type) {
	case KTAU_PROFILE:
		//dont support yet
		return -1;
		//snprintf(path, MAX_KTAU_PROC_PATH, PROFILE_PATH);
		break;
	case KTAU_TRACE:
		snprintf(path, MAX_KTAU_PROC_PATH, TRACE_PATH);
		break;
	default:
		//def is profile
		//dont support yet
		return -1;
		//snprintf(path, MAX_KTAU_PROC_PATH, PROFILE_PATH);
	}


	if((nopids == 1) && (self == 1)) /* single pid get size : use the read method */
	{
		switch(type) {
			/*
		case KTAU_PROFILE:
			cmd = CMD_SIZE;
			break;
			*/
		case KTAU_TRACE:
			cmd = CMD_TRACE_RESIZE;
			break;
		default:
			//def is profile
			//dont support yet
			return -1;
			//cmd = CMD_SIZE;
		}

		//fd = open(path,O_RDWR);	
		fd = open(path,O_RDONLY);	
		if(fd < 0)
		{
			perror("open:");
			return fd;
		}

		ktau_pkg.op.size = size;
		ktau_pkg.type = cmd;
		ktau_pkg.pid = *pid;
		ktau_pkg.nopids = nopids;
		ret_value = 0;
		
		/* CHANGED BELOW: Using ONLY ioctl now. And can only do SELF OR ALL
		 * do {
			ret_value = write(fd, &ktau_pkg, sizeof(ktau_package_t));
		} while((ret_value < 0) && (errno == EINTR));
		*/
		ret_value = ioctl(fd, cmd, &ktau_pkg);

		lc_size = ktau_pkg.op.size.size;
		if(flags)
			*flags = ktau_pkg.op.size.flags;

	}
	else //if (nopids == 0) /* ALL Pids: use IOCTL method */
	{
		switch(type) {
			/*
		case KTAU_PROFILE:
			cmd = CMD_ALL_SIZE;
			break;
			*/
		case KTAU_TRACE:
			cmd = CMD_ALL_TRACE_RESIZE;
			break;
		default:
			//def is profile
			//dont support yet
			return -1;
			//cmd = CMD_ALL_SIZE;
		}

		//fd = open(path,O_RDWR);	
		fd = open(path,O_RDONLY);	
		if(fd < 0)
		{
			perror("open:");
			return fd;
		}

		ktau_pkg.op.size = size;
		ktau_pkg.type = cmd;
		ktau_pkg.pid = -1;
		ktau_pkg.nopids = nopids;
		
		if(nopids == 0) { //then ALL processes
			hdrbuf = (char*)&ktau_pkg;
		} else { //nopids >= 1, then some other or many processes
			hdrbuf = (char*)malloc(sizeof(ktau_package_t) + (sizeof(pid_t) * nopids));
			*((ktau_package_t*)hdrbuf) = ktau_pkg;
			memcpy(hdrbuf+sizeof(ktau_package_t), pid, sizeof(pid_t)*nopids);
		}

		ret_value = 0;
		
		ret_value = ioctl(fd, cmd, hdrbuf);
		
		ktau_pkg = *((ktau_package_t*)hdrbuf);

		lc_size = ktau_pkg.op.size.size;
		if(flags)
			*flags = ktau_pkg.op.size.flags;

	}

	close(fd);

	if(ret_value < 0)
	{
		perror("read:");
		return ret_value;
	}

	ret_value = 0;

	return lc_size;
}

/* read_data: Used to query /proc/ktau for DATA of profile(s). *
 ***************************************************************
 * Arguments:
 * type:        KTAU_PROFILE or KTAU_TRACE
 *
 * self:        if reading self -> set to 1, otherwise set to zero
 *
 * pid:         pointer to a single pid , or address of array of pids
 *              Can be NULL --> request data of entire system prof
 *
 * nopids:      no of pids being pointed to above
 *              Can be ZERO --> request data of entire system prof
 *
 * buffer:      pointer to allocated memory
 *
 * size:        size of buffer (allocate memory above)
 *
 * tol:         tolerance paramter. to ignore, set to ZERO
 *
 * flags:       <TO BE DONE> . ignore, set to NULL.
 *
 * Returns:
 * On Success:  total size of profile(s) data read into buffer.
 * On Error:    -1; Other negative error codes reserved.
 *
 * Constraints:
 * Must have called read_size before to ascertain size of buffer.
 *
 * NOTE: Even if SIZE allocated is that returned by read_size, 
 * read_data can return error for lack of size -> as size of prof
 * can change after read_size. This is unlikely.
 */
long read_data(int type, int self, pid_t *pid, unsigned int nopids, char* buffer, unsigned long size, unsigned long tol, unsigned int* flags)
{
	int ret_value = 0;
	int fd = -1;
	char path[MAX_KTAU_PROC_PATH+1];
	ktau_package_t ktau_pkg;
	ktau_read_t kbuffer;
	char *lc_buf = NULL;
	char* temp_buf = NULL;
	unsigned int in_size = sizeof(ktau_package_t) + size ;
	unsigned int cmd = 0;

	//initialize buffer with user-args
	kbuffer.tol.tol_in = tol;
	kbuffer.data = buffer;
	kbuffer.size = size;
	kbuffer.flags = 0;
	if(flags)
		kbuffer.flags = *flags;

	if( !((nopids == 1) && (self == 1)) && (size < (sizeof(pid_t) * nopids)) ){
		lc_buf = (char*)malloc(sizeof(ktau_package_t) + (sizeof(pid_t) * nopids));
	} else {
		lc_buf = (char*)malloc(in_size);
	}

	if(!lc_buf)
	{
		perror("lc_buf malloc: ");
		return -1;
	}

	switch(type) {
	case KTAU_PROFILE:
		snprintf(path, MAX_KTAU_PROC_PATH, PROFILE_PATH);
		break;
	case KTAU_TRACE:
		snprintf(path, MAX_KTAU_PROC_PATH, TRACE_PATH);
		break;
	default:
		//def is profile
		snprintf(path, MAX_KTAU_PROC_PATH, PROFILE_PATH);
	}

	if((nopids == 1)&& (self == 1)) /* single pid get read : use the read method */
	{
		switch(type) {
		case KTAU_PROFILE:
			cmd = CMD_READ;
			break;
		case KTAU_TRACE:
			cmd = CMD_TRACE_READ;
			break;
		default:
			//def is profile
			cmd = CMD_READ;
		}

		//fd = open(path,O_RDWR);	
		fd = open(path,O_RDONLY);	
		if(fd < 0)
		{
			perror("open:");
			free(lc_buf);
			return fd;
		}

		ktau_pkg.op.read= kbuffer; /* so that flags/tolerance passed from app is recorded */
		ktau_pkg.type = cmd;
		ktau_pkg.pid = *pid;
		ktau_pkg.nopids = nopids; 
		
		memcpy(lc_buf, &ktau_pkg, sizeof(ktau_pkg));

		ret_value = 0;
		/* CHANGED : To Use only ioctl
		 * do {
			ret_value = write(fd, lc_buf, sizeof(ktau_package_t) + kbuffer.size);
		} while((ret_value < 0) && (errno == EINTR));
		*/
		ret_value = ioctl(fd, cmd, lc_buf);

	}
	else //if(nopids == 0) /* ALL Pids : use IOCTL */
	{
		switch(type) {
		case KTAU_PROFILE:
			cmd = CMD_ALL_READ;
			break;
		case KTAU_TRACE:
			cmd = CMD_ALL_TRACE_READ;
			break;
		default:
			//def is profile
			cmd = CMD_ALL_READ;
		}

		//fd = open(path,O_RDWR);	
		fd = open(path,O_RDONLY);	
		if(fd < 0)
		{
			perror("open:");
			free(lc_buf);
			return fd;
		}

		ktau_pkg.op.read= kbuffer; /* So that flags/tolerance passed from app is recorded */
		ktau_pkg.type = cmd;
		ktau_pkg.nopids = nopids; 

		memcpy(lc_buf, &ktau_pkg, sizeof(ktau_pkg));

		if(nopids != 0) { //then NOT ALL , but one or many other processes
			memcpy(lc_buf+sizeof(ktau_package_t), pid, sizeof(pid_t)*nopids);
		}
		
		ret_value = ioctl(fd, cmd, lc_buf);

	}
	//else /* Mamny pids : We dont support */
	//{
	//	perror("read_data: Many PIDs not supported. Only Single PID & ALL Pids.");
	//	ret_value = -1;
	//}

	close(fd);

	if(ret_value < 0)
	{
		perror("read/ioctl ret val < 0:");
		if(lc_buf)
		{
			free(lc_buf);
		}
		return ret_value;
	}

	temp_buf = kbuffer.data;
	kbuffer = (*(ktau_package_t*)lc_buf).op.read;
	kbuffer.data = temp_buf;

	memcpy(kbuffer.data, (char*)(lc_buf + sizeof(ktau_package_t)), ret_value - sizeof(ktau_package_t));

	if(flags)
		*flags = kbuffer.flags;

	ret_value = 0;
	
	free(lc_buf);

	return kbuffer.size;
}


/* purge_data: Used to reset state of profiles /proc/ktau for pid(s). *
 **********************************************************************
 * Arguments:
 * pid:         pointer to a single pid , or address of array of pids
 *              Can be NULL --> request reset of entire system prof
 *
 * nopids:      no of pids being pointed to above
 *              Can be ZERO --> request reset of entire system prof
 *
 * tol:         tolerance paramter. to ignore, set to ZERO
 *
 * flags:       <TO BE DONE> . ignore, set to NULL.
 *
 * Returns:
 * On Success:  0
 * On Error:    -1; Other negative error codes reserved.
 *
 * Constraints:
 * None.
 */
long purge_data(pid_t *pid, unsigned int nopids, unsigned long tol, unsigned int* flags)
{
	int ret_value = 0;
	int fd = -1;
	char path[MAX_KTAU_PROC_PATH+1];
	ktau_package_t ktau_pkg;
	ktau_write_t kbuffer;
	char* lc_buf = NULL;
	char* temp_buf = NULL;

	//initialize buffer with user-args
	kbuffer.tol.tol_in = tol;
	kbuffer.flags = 0;
	if(flags)
		kbuffer.flags = *flags;

	lc_buf = (char*)malloc(sizeof(ktau_package_t));
	if(!lc_buf)
	{
		perror("malloc:");
		return -1;
	}

	if(nopids == 1) /* single pid write : use the write method */
	{
		/* CHANGED: TO use only all & ioctl 
		 * snprintf(path, MAX_KTAU_PROC_PATH, "/proc/ktau/%u", *pid);
		*/
		snprintf(path, MAX_KTAU_PROC_PATH, "/proc/ktau/all");

		//fd = open(path,O_RDWR);	
		fd = open(path,O_RDONLY);	
		if(fd < 0)
		{
			perror("open:");
			free(lc_buf);
			return fd;
		}

		ktau_pkg.op.write = kbuffer; /* so that flags/tolerance passed from app is recorded */
		ktau_pkg.type = CMD_PURGE;
		ktau_pkg.pid = *pid;
		
		memcpy(lc_buf, &ktau_pkg, sizeof(ktau_pkg));

		ret_value = 0;
		/* CHANGED: To use ioctl ONLY
		 * do {
			ret_value = write(fd, lc_buf, sizeof(ktau_package_t));
		} while((ret_value < 0) && (errno == EINTR));
		*/
		ret_value = ioctl(fd, CMD_PURGE, lc_buf);

		ktau_pkg = *((ktau_package_t*)lc_buf);
	}
	else if(nopids == 0) /* ALL Pids : use IOCTL */
	{
		snprintf(path, MAX_KTAU_PROC_PATH, "/proc/ktau/all");

		//fd = open(path,O_RDWR);	
		fd = open(path,O_RDONLY);	
		if(fd < 0)
		{
			perror("open:");
			free(lc_buf);
			return fd;
		}

		ktau_pkg.op.write = kbuffer; /* So that flags/ tolerance passed from app is recorded */
		ktau_pkg.type = CMD_ALL_PURGE;
		
		memcpy(lc_buf, &ktau_pkg, sizeof(ktau_pkg));

		ret_value = ioctl(fd, CMD_ALL_PURGE, lc_buf);

		ktau_pkg = *((ktau_package_t*)lc_buf);
	}
	else /*Many pids : not supported */
	{
		perror("pruge_data: Many PIDs not supported. Only Single & ALL Pids.");
		ret_value = -1;
	}

	close(fd);

	if(ret_value < 0)
	{
		perror("write / ioctl:");
		if(lc_buf)
		{
			free(lc_buf);
		}
		return ret_value;
	}

	kbuffer = ktau_pkg.op.write;

	if(flags)
		*flags = kbuffer.flags;

	ret_value = 0;
	
	free(lc_buf);

	return ret_value;
}


/* ktau_set_state : Used to query /proc/ktau for SIZE of profile(s). *
 ***************************************************************
 * Arguments:
 * pid:         pointer to a single pid (MUST BE SELF)
 *              Can be NULL (if pid is unknown, e.g. for threads) 
 *
 * state:       PTR to ktau_state (CAN be NULL, to Unset) (buffer must have been atleats a page long
 *
 * flags:       <TO BE DONE> . ignore, set to NULL.
 *
 * Returns:
 * On Success:  Zero.
 * On Error:    -1; Other negative error codes reserved.
 *
 * Constraints:
 * ktau_set_state only works on 'SELF' , cannot perform it (yet) on 
 * other processes.
 */
long ktau_set_state(pid_t *pid, ktau_state* state, unsigned int* flags)
{
	int ret_value = 0;
	int fd = -1;
	char path[MAX_KTAU_PROC_PATH+1];
	ktau_package_t ktau_pkg;
	ktau_merge_t kbuffer;

	//initialize buffer with user-args
	kbuffer.flags = 0;
	if(flags)
		kbuffer.flags = *flags;

	kbuffer.ppstate = state;

	snprintf(path, MAX_KTAU_PROC_PATH, PROFILE_PATH);

	//fd = open(path,O_RDWR);	
	fd = open(path,O_RDONLY);	
	if(fd < 0)
	{
		perror("open:");
		return fd;
	}

	ktau_pkg.op.merge = kbuffer; /* so that flags/tolerance passed from app is recorded */
	ktau_pkg.type = CMD_SET_MERGE;
	if(pid) {
		ktau_pkg.pid = *pid;
	} else {
		ktau_pkg.pid = -1;
	}
	
	ret_value = ioctl(fd, CMD_SET_MERGE, (char*)(&ktau_pkg));

	close(fd);

	if(ret_value < 0)
	{
		perror("ioctl:");
	}
	else 
	{
		ret_value = 0;
	}

	return ret_value;
}
#endif /* OLD CODE - K1 */

/* aggr_many_profiles: Aggregates array of Profiles to provide single kernel-as-a-whole profile 
 *****************************************************************
 * Arguments:
 * inprofiles:  ktau_output ptr to the Unpacked profile data.
 *
 * no_profiles: the number of profiles pointed to by profiles* (above)
 *
 * max_prof_entries: the maximum no of entries a single profile may have
 *
 * outprofile:  ptr to allocated memory for one profile with max_prof_size entries
 *
 * Returns:     0 on success, negative on error
 * 
 * Constraints:
 * Can aggregate from only ktau_output (i.e unpacked profiles). Cannot 
 * aggregate packed-binary profile data --> therefore unpack_bindata
 * must be called on those before calling this.
 */
int aggr_many_profiles(const ktau_output* inprofiles, unsigned int no_profiles, unsigned int max_prof_entries, ktau_output* outprofile)
{
	int ret_val = 0, profno = 0, entno = 0, cur_index = 0;
	const ktau_output* curr_prof = NULL;
	h_ent *out_hent, *cur_hent;
	o_ent * out_oent = NULL;
	int maxindex = 0;

	for(profno = 0; profno<no_profiles; profno++) {
		curr_prof = (inprofiles + profno);
		for(entno = 0; entno < curr_prof->size; entno++) {
			cur_index = (curr_prof->ent_lst + entno)->index;
			cur_hent = &((curr_prof->ent_lst + entno)->entry);
			out_hent = &((outprofile->ent_lst + cur_index)->entry);
			out_oent = (outprofile->ent_lst + cur_index);
			out_oent->index = cur_index;
			out_hent->addr = cur_hent->addr;
			//assume data is timer for now!
			if(cur_hent->type == KTAU_TIMER) {
				out_hent->data.timer.count += cur_hent->data.timer.count;
				out_hent->data.timer.incl += cur_hent->data.timer.incl;
				out_hent->data.timer.excl += cur_hent->data.timer.excl;
			}
		}

		if(maxindex < cur_index) {
			maxindex = cur_index;
		}
	}
	
	outprofile->size = maxindex;

	return ret_val;
}




/* Once Read (into Buffer), that Data needs to be *
 * expanded into Profile Data.                    *
 * type:        KTAU_TYPE_PROFILE or KTAU_TYPE_TRACE
 *
 * Returns: no profiles expanded. -1 on error.
 *
 * Note: To free memory allocated to **output.
 * must call unpack_free. Otherwise will leak.
 */
long unpack_bindata(int type, char* buffer, unsigned long size, ktau_output** output) 
{
        int alloc_size = 0;
        int i = 0;
        char *bufptr = 0x0;
	int noprofs = -1;

	bufptr = buffer;

	memcpy(&noprofs, buffer, sizeof(unsigned int));	//<for no-of-profiles>
	bufptr+=sizeof(unsigned int);

	*output = (ktau_output*)malloc(noprofs * sizeof(ktau_output));
        if(!*output) {
                perror("malloc failed");
                return -1;
        }

        for(i = 0; i< noprofs; i++)
        {
		memcpy(&((*output)[i].pid), bufptr, sizeof(pid_t)); //<for pid>
		bufptr+=sizeof(pid_t);
		memcpy(&((*output)[i].size), bufptr, sizeof(unsigned int)); //<for size>
		bufptr+=sizeof(unsigned int);

		if(type == KTAU_TYPE_PROFILE) {

			(*output)[i].ent_lst = (o_ent*)malloc(sizeof(o_ent)*(*output)[i].size);
			if(!((*output)[i].ent_lst)) {
				perror("malloc failed");
				return -1;
			}
			memcpy((*output)[i].ent_lst, bufptr, sizeof(o_ent) * (*output)[i].size); //<for o_ent>
			bufptr+= (sizeof(o_ent) * (*output)[i].size);

		} else if(type == KTAU_TYPE_TRACE) {

			(*output)[i].trace_lst = (ktau_trace*)malloc(sizeof(ktau_trace)*(*output)[i].size);
			if(!((*output)[i].trace_lst)) {
				perror("malloc failed");
				return -1;
			}
			memcpy((*output)[i].trace_lst, bufptr, sizeof(ktau_trace) * (*output)[i].size); //<for ktau_trace>
			bufptr+= (sizeof(ktau_trace) * (*output)[i].size);

		}
        }

        return noprofs;
}

void unpack_free(int type, ktau_output** output, int noprofs) {
	int i = 0;
	for(i = 0; i< noprofs; i++) {
		if(type == KTAU_TYPE_PROFILE) {
			free((*output)[i].ent_lst);
		} else if(type == KTAU_TYPE_TRACE) {
			free((*output)[i].trace_lst);
			(*output)[i].trace_lst = NULL;
		} else {
			printf("unpack_free: Error, unknown output Type (not KTAU_TYPE_PROFILE or KTAU_TYPE_TRACE).\n");
			return;
		}
		(*output)[i].trace_lst = (*output)[i].ent_lst = NULL;
	}
	free(*output);
	*output = NULL;

	return;
}

/* Routine prints out an unpacked ktau_output       *
 * to the provided file-stream (can be stdout etc). *
 * Expects a const ptr to a single ktau_output */

/* Helper */
#ifdef __cplusplus
extern "C" {
#endif
static void print_o_ent(FILE* fp, o_ent* pent, unsigned int flags);
static void print_trace(FILE* fp, ktau_trace* ptrace);
#ifdef __cplusplus
}
#endif


 /* type:        KTAU_TYPE_PROFILE or KTAU_TYPE_TRACE
 */
void print_many_profiles(int type, FILE* fp, const ktau_output* profiles, unsigned int no_profiles)
{
	int i = 0;

	fprintf(fp, "No Profiles: %u\n", no_profiles);
	for(i=0; i< no_profiles; i++)
	{
		print_ktau_output(type, fp, &(profiles[i]));
	}
}

void _print_ktau_output(int type, FILE* fp, const ktau_output* profile, unsigned int flags)
{
	o_ent* pent = NULL;
	ktau_trace* ptrace = NULL;
	int i = 0;

	if((!profile) || (!fp))
	{
		perror("Input Null ptr. \n");
		return;
	}

	fprintf(fp, "PID: %d\t No Entries: %u\n", profile->pid, profile->size);

	for(i=0; i<profile->size; i++)
	{
		if(type == KTAU_TYPE_PROFILE) {

			pent = &(profile->ent_lst[i]);
			print_o_ent(fp, pent,flags);

		} else if(type == KTAU_TYPE_TRACE) {

			ptrace = &(profile->trace_lst[i]);
			print_trace(fp, ptrace);

		}
	}

}

/* helper to print o_ents */
static void print_o_ent(FILE* fp, o_ent* pent,unsigned int flags)
{
	h_ent* ph_ent = NULL;
	int i = 0;
	ktau_timer* timer;
	int index = 0;

	if(!fp || !pent)
	{
		perror("Null input. \n");
		return;
	}

	index = pent->index;

	ph_ent = &(pent->entry);

	if(!ph_ent)
	{
		fprintf(fp, "NULL\n");
		return;
	}

	timer = &(ph_ent->data.timer);

        if(flags != 0){
                fprintf(fp, "Entry %4d: addr %x, count %4u, incl %4llu, excl %4llu\n",
                                        index,
                                        ph_ent->addr,
                                        timer->count,
                                        timer->incl,
					timer->excl);
	}else{
                fprintf(fp, "Entry %4d: addr %x, count %4u, incl %4u, excl %4u\n",
                                        index,
                                        ph_ent->addr,
                                        0,
                                        0,
					0);
	}

	return;
}

/* helper to print ktau_traces */
static void print_trace(FILE* fp, ktau_trace* ptrace)
{
	int i = 0;

	if(!fp || !ptrace)
	{
		perror("Null input. \n");
		return;
	}

        fprintf(fp, "%llu %x %u\n",
                                        ptrace->tsc,
                                        ptrace->addr,
                                        ptrace->type);

	return;
}


/*
long ktau_dump_toggle()
{
        int ret_value = 0;
        int fd = -1;
        char path[MAX_KTAU_PROC_PATH+1];
        ktau_package_t ktau_pkg;
        unsigned long lc_size = 0;

        snprintf(path, MAX_KTAU_PROC_PATH, "/proc/ktau/all");

        fd = open(path,O_RDWR); 
        if(fd < 0)
        {
                perror("open:");
                return fd;
        }

        ktau_pkg.type = CMD_SET_DUMP;
        ret_value = 0;
        
        ret_value = ioctl(fd, CMD_SET_DUMP, &ktau_pkg);

        close(fd);

        if(ret_value < 0)
        {
                perror("ioctl:");
        }

        return ret_value;
}
*/


/* BinData From File                              *
 * expanded into Profile Data.                    *
 * Returns: no profiles expanded. -1 on error.    *
 * Contraint: Caller needs to free ktau_output mem*
 */
long unpack_bindata_file(int type, char* path, ktau_output** output)
{
        int size = 0, i = 0, fd = -1, noprofs = 0;
        long ret_value = -1;
        struct stat statbuf;
        char* buffer = NULL, *mv_buffer = NULL;

        if(!path) {
                return -1;
        }

	/* Get the size of the file on disk *
	 * prior to reading the file.	    *
	 *----------------------------------*/
        if(stat(path, &statbuf) != 0) {
                perror("stat ret error.\n");
                return -1;
        }

        size = statbuf.st_size;

	/* allcate 'size' bytes of buffer */
        buffer = (char*)malloc(size);
        if(!buffer) {
                perror("malloc failed.\n");
                return -1;
                //exit(-1);
        }

        fd = open(path, O_RDONLY);
        if(fd <= 0) {
                perror("open failed.\n");
                return -1;
                //exit(-1);
        }

        ret_value = read(fd, buffer, size);
        if(ret_value < size) {
                perror("read: less than required.\n");
                //exit(-1);
        }

	//CHECK THIS - KTAU2 doesnt move the buffer...
        //mv_buffer = buffer + sizeof(ktau_package_t);

	//CHECK THIS - KTAU2 doesnt move the buffer...
        /* Unpack Data*/
        //noprofs = unpack_bindata(type, mv_buffer, size, output);
        noprofs = unpack_bindata(type, buffer, size, output);

        free(buffer);

        return noprofs;
}


