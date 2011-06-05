/*************************************************************
 * File         : kernel/ktau/ktau_linux-2.4.c
 * Version      : $Id: ktau_linux-2.4.c,v 1.3 2006/11/12 00:26:29 anataraj Exp $
 * NOTE		: This file is to be included when compiling for
 *		  Linux-2.4
 *
 ************************************************************/

#ifndef _KTAU_LINUX_2_4_C_
#define _KTAU_LINUX_2_4_C_

#include <linux/version.h>
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0) ) 

/********************************************************************************/
/* 
 * This is defined in the Linux-2.6 but not Linux-2.4 
 * NOTE: Moved from ktau_hash.c
 */
#ifndef for_each_process(p)
#define for_each_process(p) for_each_task(p)
#endif /*for_each_process(p)*/

/*
 * This function is from linux-2.6 kernel since it does not
 * exist in linux-2.4.
 * NOTE: Moved from ktau_inst.c 
 */
#ifdef CONFIG_SMP
#ifndef LOCK 
#define LOCK "lock ; "
#endif
#endif

/********************************************************************************/
/*
 * This macro and fuction does not exist in 2.4.x for 
 * X86 architecture, but does exist for PPC. 
 */
#ifndef CONFIG_PPC32
#ifndef atomic_inc_return(v)
#define atomic_inc_return(v)    (atomic_add_return(1,v))
#endif

static __inline__ int atomic_add_return(int i, atomic_t *v)
{
        int __i;
#ifdef CONFIG_M386
        if(unlikely(boot_cpu_data.x86==3))
                goto no_xadd;
#endif

        /* Modern 486+ processor */
        __i = i;
        __asm__ __volatile__(
                LOCK "xaddl %0, %1;"
                :"=r"(i)
                :"m"(v->counter), "r"(i));
        return i + __i;

#ifdef CONFIG_M386
no_xadd: /* Legacy 386 processor */
        local_irq_disable();
        __i = atomic_read(v);
        atomic_set(v, i + __i);
        local_irq_enable();
        return i + __i;
#endif /*CONFIG_M386*/
}
#endif /*CONFIG_PPC32*/
/********************************************************************************/

#endif /* ( LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0) ) */
#endif /* _KTAU_LINUX_2_4_C_ */
