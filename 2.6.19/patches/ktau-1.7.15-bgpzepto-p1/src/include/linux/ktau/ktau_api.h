/************************************************
 * File		: include/linux/ktau/ktau_api.h
 * Version	: $Id: ktau_api.h,v 1.9.2.1 2007/03/10 02:13:38 anataraj Exp $
 ***********************************************/
#ifndef _KTAU_API_H_
#define _KTAU_API_H_

#include "ktau_datatype.h"
#include "ktau_hash.h"
#include "ktau_cont.h"
/* 
 * OUT APIs : These will be used by ktau_proc module
 *            to get and reset profile data.
 */

/*
 * Description	: Dump profile data into a provided array
 *                Used only for 'own' process
 * Input	: buf = ktau_prof pointer for the pid
 * 		  size = size of output array 
 * 		  arr = output array of type h_ent
 * Output	: size of element in the array
 * 		  -1 = error
 * Constrain	: User must deallocate the memory
 */
extern int dump_hash_self(struct task_struct* tsk, ktau_prof *buf, ktau_output *out, unsigned long long tol);
//extern int dump_hash_other(ktau_prof *buf, ktau_output *out, unsigned long long tol);

/*
 * Description	: Reset profile data of a pid
 *                Used only for 'own' process
 * Input	: buf = ktau_prof pointer for the pid
 * Output	: -1 = error
 */
extern int purge_hash_self(ktau_prof *buf, unsigned long long tol);
//extern int purge_hash_other(ktau_prof *buf, unsigned long long tol);

/*
 * Description	: Dump profile data into a provided array
 *                Used for ANY process(es)
 * Input	: pid_lst = pointer to arr of pids
 *                - Can be null, if ALL Pids required
 * 		  size = size of output array 
 *                - Can be 0 if aLL Pids required
 * 		  arr = output array of type h_ent
 * Output	: size of element in the array of
 * 		  ktau_output
 * 		  -1 = error
 * Constrain	: User must deallocate the memory
 */
extern int dump_hash_many(pid_t *pid_lst,unsigned int size, ktau_output **arr, unsigned long long tol);
extern int purge_hash_many(pid_t *pid_lst,unsigned int size, unsigned long long tol);

/*----------------------------------------------------------------------------*/
/* MERGE API */
/* 
 * CONTRL APIs : These will be used by ktau_proc module
 *            to 'control'/augment behaviour of profiling.
 */

extern int set_hash_userstate(ktau_prof *buf, ktau_state** ppstate);

extern int set_hash_userstate_2(ktau_prof *buf, ktau_state* pstate);

/*----------------------------------------------------------------------------*/
/* TRACE API */

extern int dump_trace_self(ktau_prof *ktau, ktau_output *out);
extern int dump_trace_many(pid_t *pid_lst,unsigned int size, ktau_output **arr);
extern int get_trace_size(int* num_pid);
extern int get_profile_size_self(ktau_prof *buf);
extern int get_profile_size_many(pid_t *pid_lst, int* num_pid);

/* ktau_reg_ctrevents: find and register with events that ctr interested in	*
 * , use the default shared-ctr handler - for now. 	
 * Returns +ve number of events succesfully registered.
 * This no may not be equal to no_syms -- some maybe pending.
 * If its zero - then is it an error? not necessarily..
 * -ve is error. */
int ktau_reg_ctrevents(struct task_struct* tsk, ktau_sh_ctr* k_shctr, KTAU_EVHDR_CTR ev_handler);

/* ktau_unreg_ctrevents: find and unregister events 
 * -ve is error.  0 Success.*/
int ktau_unreg_ctrevents(struct task_struct* tsk, ktau_sh_ctr* k_shctr);
#endif /* _KTAU_API_H_ */
