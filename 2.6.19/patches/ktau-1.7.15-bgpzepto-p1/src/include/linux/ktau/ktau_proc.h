/**************************************************************************
 * File		: include/linux/ktau/ktau_proc.h
 * Version 	: $Id: ktau_proc.h,v 1.8.2.1 2007/02/25 10:18:43 anataraj Exp $
 *************************************************************************/
#ifndef _KTAU_PROC_H
#define _KTAU_PROC_H

/* 
 * declaration to avoid including sched.h 
 */
struct task_struct;

/* 
 * /proc/ktau stuff needed by the rest of the ktau in kernel 
 */

/* Initialization routines *
 *-------------------------*/
void __init ktau_init(void);

/* 
 * to be used to create a new /proc/ktau/<pid> 
 */
extern void setup_proc_ktau(struct task_struct *tsk);
extern void destroy_proc_ktau(pid_t taupid);

#endif /* _KTAU_PROC_H */
