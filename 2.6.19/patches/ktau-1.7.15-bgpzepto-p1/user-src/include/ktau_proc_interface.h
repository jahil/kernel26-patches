/***********************************************
 * File		: include/linux/ktau/ktau_proc_interface.h
 * Version	: $Id: ktau_proc_interface.h,v 1.6.2.7 2007/08/08 19:57:20 anataraj Exp $
 ***********************************************/

#ifndef _KTAU_PROC_INTERFACE_H
#define _KTAU_PROC_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

/* User-Space Application Interface Functions */

/* u32 and u8 are not defined in user-space.	*
 * But these are used in ktau kernel structs.	*
 * So we manually typedef them here.		*
 * THIS IS A HACK: ITS ARCH DEPENDANT AND CAN	*
 * BREAK.					*
 *----------------------------------------------*/
typedef unsigned int u32;
typedef unsigned char u8;

/* ktau specific KERNEL headers */
#include <linux/ktau/ktau_datatype.h>	/* ktau_output etc */
#include <linux/ktau/ktau_proc_external.h> /* IOCTL Cmd Defs etc */
/* 
 * HACK for ktaud 
 * for getting sizeof(ktau_package_t)
 * This should not be used directly
 */
#include <linux/ktau/ktau_proc_data.h> /* IOCTL Cmd Defs etc */

/* user clib headers */
#include <stdio.h>

extern long ktau_dump_toggle();

//#define KTAU_PROFILE		0
//#define KTAU_TRACE		1

enum {KTAU_SHCTR_TYPE_EXCL=5, KTAU_SHCTR_TYPE_INCL, KTAU_SHCTR_TYPE_NUM, KTAU_SHCTR_TYPE_MAX};

/* Function(s) to access /proc to read size,data,purge & set_state . */

/* Read & Purge PIDs 				    *
 * If nopids > 1, then used for multiple pids       *
 * if nopids = 0 & pid=null, then used for ALL Pids *
 */
long ktau_noop_timed(unsigned long long* timings);

/* read_size: Used to query /proc/ktau for SIZE of profile(s). *
 ***************************************************************
 * Arguments:
 * type:	KTAU_PROFILE or KTAU_TRACE
 *
 * self:        if reading self -> set to 1, otherwise set to zero
 *
 * pid:		pointer to a single pid , or address of array of pids
 *              Can be NULL (if nopids is ZERO) --> size of FULL sys. prof
 *
 * nopids:	no of pids being pointed to above
 * 		Can be ZERO --> request size of entire system prof
 *
 * tol:		tolerance paramter. to ignore, set to ZERO
 *
 * pflags:	<TO BE DONE> . ignore, set to NULL.
 *
 * compratio:  This is a fraction between 0 & 1 (can be zero or 1 also)
 *              -1 --> means USE DEFAULT VALUE
 *              Compensation-Ratio Used to scale-up the size returned 
 *              from /proc, so that when a call is made to read_data, 
 *              the size is larger to accomodate any changes to profile. 
 *              NOTE: To Ignore SET to -1. 
 *
 * Returns:
 * On Success:	total size of profile(s) of pid(s)
 * On Error:	-1; Other negative error codes reserved.
 *
 * Constraints:
 * None.
 */
extern long read_size(int type, int self, pid_t *pid, unsigned int nopids, unsigned long tol, unsigned int* pflags, float compratio);
extern long read_size_timed(int type, int self, pid_t *pid, unsigned int nopids, unsigned long tol, unsigned int* pflags, float compratio, unsigned long long* timings);

/* ktau_trace_resize: Used to resize trace-buffer of kernel *
 ***************************************************************
 * Arguments:
 * newsize:     no of entries to resize into (not memory size)
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
 * pflags:       <TO BE DONE> . ignore, set to NULL.
 *
 * Returns:
 * On Success:  total size of profile(s) of pid(s)
 * On Error:    -1; Other negative error codes reserved.
 *
 * Constraints:
 * None.
 */
extern long ktau_trace_resize(int newsize, int type, int self, pid_t *pid, unsigned int nopids, unsigned long tol, unsigned int* pflags);


/* read_data: Used to query /proc/ktau for DATA of profile(s). *
 ***************************************************************
 * Arguments:
 * type:	KTAU_PROFILE or KTAU_TRACE
 *
 * self:        if reading self -> set to 1, otherwise set to zero
 *
 * pid:		pointer to a single pid , or address of array of pids
 * 		Can be NULL --> request data of entire system prof
 *
 * nopids:	no of pids being pointed to above
 * 		Can be ZERO --> request data of entire system prof
 *
 * buffer:	pointer to allocated memory
 *
 * size:	size of buffer (allocate memory above)
 *
 * tol:		tolerance paramter. to ignore, set to ZERO
 *
 * pflags:	<TO BE DONE> . ignore, set to NULL.
 *
 * Returns:
 * On Success:	total size of profile(s) data read into buffer.
 * On Error:	-1; Other negative error codes reserved.
 *
 * Constraints:
 * Must have called read_size before to ascertain size of buffer.
 *
 * NOTE: Even if SIZE allocated is that returned by read_size, 
 * read_data can return error for lack of size -> as size of prof
 * can change after read_size. This is unlikely.
 */
extern long read_data(int type, int self, pid_t *pid, unsigned int nopids, char* buffer, unsigned long size, unsigned long tol, unsigned int* pflags);
extern long read_data_timed(int type, int self, pid_t *pid, unsigned int nopids, char* buffer, unsigned long size, unsigned long tol, unsigned int* pflags, unsigned long long* timings);


/* purge_data: Used to reset state of profiles /proc/ktau for pid(s). *
 **********************************************************************
 * Arguments:
 * pid:		pointer to a single pid , or address of array of pids
 * 		Can be NULL --> request reset of entire system prof
 *
 * nopids:	no of pids being pointed to above
 * 		Can be ZERO --> request reset of entire system prof
 *
 * tol:		tolerance paramter. to ignore, set to ZERO
 *
 * flags:	<TO BE DONE> . ignore, set to NULL.
 *
 * Returns:
 * On Success:	0
 * On Error:	-1; Other negative error codes reserved.
 *
 * Constraints:
 * None.
 */
extern long purge_data(pid_t *pid, unsigned int nopids, unsigned long tol, unsigned int* flags);


/* write_data: Used to write into state of profiles /proc/ktau for pid(s).
 **********************************************************************
 * NOTE:	CURRENTLY UNIMPLEMENTED. DO NOT USE.
 * Arguments:
 * Returns:
 * Constraints:
 */
extern long write_data(pid_t *pid, unsigned int nopids, char* buffer, unsigned long size, unsigned long tol, unsigned int* flags);


/* ktau_set_state : Used to query /proc/ktau for SIZE of profile(s). *
 ***************************************************************
 * Arguments:
 * pid:		pointer to a single pid (MUST BE SELF)
 *              Can be NULL (if pid is unknown, e.g. for threads) 
 *
 * pflags:	<TO BE DONE> . ignore, set to NULL.
 *
 * Returns:
 * On Success:	Zero.
 * On Error:	-1; Other negative error codes reserved.
 *
 * Constraints:
 * set_state only works on 'SELF' , cannot perform it (yet) on 
 * other processes.
 */
extern long ktau_set_state(pid_t *pid, ktau_state* state, unsigned int* pflags);

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
extern ktau_ushcont* ktau_add_container(int type, int self, pid_t *pid, unsigned int nopids, unsigned int* pflags, unsigned long size);

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
extern int ktau_del_container(int type, int self, pid_t *pid, unsigned int nopids, unsigned int* pflags, ktau_ushcont* pushcont);


/* ktau_del_container_user:
 * Same as ktau_del_container, except this only deletes the container in user-space *
 * and does not inform the kernel about this change - in effect assumes that a fork *
 * occurred and that the mem-map in OS of new process does not have shared-mapping. *
 *
 * NOTE: Dont call if this isnt right after a fork. Specifically implemented for    *
 * calling from TAU, not to be used otherwise - effects undefined.                  */
extern int ktau_del_container_user(int type, int self, pid_t *pid, unsigned int nopids, unsigned int* pflags, ktau_ushcont* pushcont);


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
extern int ktau_get_counter(ktau_ushcont* pushcont, char name[KTAU_MAX_COUNTERNAME], int no_syms, unsigned long* syms, ktau_ush_ctr** ppctr);

/* ktau_put_counter: de-registers and deletes existing shared-ctr    *
 ***************************************************************
 * Arguments:
 * pshctr:	ptr to ktau_ush_ctr obtained from ktau_get_counter
 */
extern int ktau_put_counter(ktau_ush_ctr* pushctr);

/* ktau_put_counter_user: Same as ktau_put_counter,            *
 * but does not do OS de-registration.                         *
 * NOTE: To be used from inside TAU right after a fork ONLY.   *
 ***************************************************************
 * Arguments:
 * pshctr:	ptr to ktau_ush_ctr obtained from ktau_get_counter
 */
extern int ktau_put_counter_user(ktau_ush_ctr* pushctr);

/* ktau_copy_counter: Copies from shared-ctr into
 * user-provided buffer of ktau_data[] 
 * Returns no of data copied on Success. -ve on Error.
 */ 
extern int ktau_copy_counter(ktau_ush_ctr* pushctr, ktau_data* data, int no_data);

/* ktau_copy_counter_excl: Copies exclusive val from shared-ctr into
 * user-provided buffer of unsigned long long
 * Returns no of data copied on Success. -ve on Error.
 */ 
extern int ktau_copy_counter_excl(ktau_ush_ctr* pushctr, unsigned long long* data, int no_data);

/* ktau_copy_counter_incl: Copies inclusive val from shared-ctr into
 * user-provided buffer of unsigned long long
 * Returns no of data copied on Success. -ve on Error.
 */ 
extern int ktau_copy_counter_incl(ktau_ush_ctr* pushctr, unsigned long long* data, int no_data);

/* ktau_copy_counter_count: Copies count val (num of times event occured) from shared-ctr into
 * user-provided buffer of unsigned long long
 * Returns no of data copied on Success. -ve on Error.
 */ 
extern int ktau_copy_counter_count(ktau_ush_ctr* pushctr, unsigned long long* data, int no_data);

/* ktau_copy_counter_type: Copies val based on type (num or excl or incl) from shared-ctr into
 * user-provided buffer of unsigned long long
 * Returns no of data copied on Success. -ve on Error.
 */ 
extern int ktau_copy_counter_type(ktau_ush_ctr* pushctr, unsigned long long* data, int no_data, int* cType);


/* Function(s) to convert data read from kernel. */

/* unpack_bindata: Once Read (into Buffer) by calling read_data, 
 * that Data needs to be expanded into Profile Data. Expansion is
 * required as its read from kernel-space /proc as contiguous 
 * binary data.
 *****************************************************************
 * Arguments:
 * type:	KTAU_PROFILE or KTAU_TRACE
 *
 * buffer:	buffer containing the binary (packed) data read using
 * 		read_data.
 *
 * size:	size of above buffer.
 *
 * output:	pointer to an UN-Allocated ktau_output* . [this function
 * 		will allocate *output.]
 *
 * Returns:
 * On Success:	no of profiles unpacked & pointed to by output.
 * On Error:	-1; Other negative error codes reserved.
 *
 * Constraints:
 * Caller MUST De-Allocate (*output) using 'free'.
 */
extern long unpack_bindata(int type, char* buffer, unsigned long size, ktau_output** output);


extern void unpack_free(int type, ktau_output** output, int noprofs);

/* aggr_many_profiles: Aggregates array of Profiles to provide single kernel-as-a-whole profile 
 *****************************************************************
 * Arguments:
 * inprofiles:	ktau_output ptr to the Unpacked profile data.
 *
 * no_profiles:	the number of profiles pointed to by profiles* (above)
 *
 * max_prof_entries: the maximum no of entries a single profile may have
 *
 * outprofile:	ptr to allocated memory for one profile with max_prof_size entries
 *
 * Returns:	0 on success, negative on error
 * 
 * Constraints:
 * Can aggregate from only ktau_output (i.e unpacked profiles). Cannot 
 * aggregate packed-binary profile data --> therefore unpack_bindata
 * must be called on those before calling this.
 */
extern int aggr_many_profiles(const ktau_output* inprofiles, unsigned int no_profiles, unsigned int max_prof_entries, ktau_output* outprofile);


/* Formatting Functions: Data Dumping & On-Screen Formatted Output */

/* print_many_profiles: Prints array of Profiles to file-stream (can be stdout) 
 *****************************************************************
 * Arguments:
 * type:	KTAU_PROFILE or KTAU_TRACE
 *
 * fp:	FILE pointer to an open, writable file-stream. This can be any 
 * 	output file-stream, including stdout/sdterr.
 *
 * profiles:	ktau_output ptr to the Unpacked profile data.
 *
 * no_profiles:	the number of profiles pointed to by profiles* (above)
 *
 * Returns:	void
 * 
 * Constraints:
 * Can print from only ktau_output (i.e unpacked profiles). Cannot 
 * print packed-binary profile data --> therefore unpack_bindata
 * must be called on those before calling this.
 */
extern void print_many_profiles(int type, FILE* fp, const ktau_output* profiles, unsigned int no_profiles);


/* print_ktau_output: Prints A SINGLE Profile to file-stream (can be stdout) 
 *****************************************************************
 * Arguments:
 * type:	KTAU_PROFILE or KTAU_TRACE
 *
 * fp:	FILE pointer to an open, writable file-stream. This can be any 
 * 	output file-stream, including stdout/sdterr.
 *
 * profile:	ktau_output ptr to the Unpacked profile data.
 *
 * Returns:	void
 * 
 * Constraints:
 * Can print from only ktau_output (i.e unpacked profile). Cannot 
 * print packed-binary profile data --> therefore unpack_bindata
 * must be called on those before calling this.
 */
#define print_ktau_output(type, fp, profile) _print_ktau_output(type,fp,profile,1)
extern void _print_ktau_output(int type, FILE* fp, const ktau_output* profile, unsigned int flags);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  /*_KTAU_PROC_INTERFACE_H */
