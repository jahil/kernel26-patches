/* $Id: virtual_stub.c,v 1.26.2.6 2007/04/09 14:18:34 mikpe Exp $
 * Kernel stub used to support virtual perfctrs when the
 * perfctr driver is built as a module.
 *
 * Copyright (C) 2000-2007  Mikael Pettersson
 */
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
#include <linux/config.h>
#endif
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/perfctr.h>
#include "compat.h"

static void bug_void_perfctr(struct vperfctr *perfctr)
{
	current->thread.perfctr = NULL;
	BUG();
}

#ifdef CONFIG_PERFCTR_CPUS_FORBIDDEN_MASK
static void bug_set_cpus_allowed(struct task_struct *owner, struct vperfctr *perfctr, cpumask_t new_mask)
{
	owner->thread.perfctr = NULL;
	BUG();
}
#endif

struct vperfctr_stub vperfctr_stub = {
	.exit = bug_void_perfctr,
	.suspend = bug_void_perfctr,
	.resume = bug_void_perfctr,
	.sample = bug_void_perfctr,
#ifdef CONFIG_PERFCTR_CPUS_FORBIDDEN_MASK
	.set_cpus_allowed = bug_set_cpus_allowed,
#endif
};

/*
 * exit_thread() calls __vperfctr_exit() via vperfctr_stub.exit().
 * If the process' reference was the last reference to this
 * vperfctr object, and this was the last live vperfctr object,
 * then the perfctr module's use count will drop to zero.
 * This is Ok, except for the fact that code is still running
 * in the module (pending returns back to exit_thread()). This
 * could race with rmmod in a preemptive UP kernel, leading to
 * code running in freed memory. The race also exists in SMP
 * kernels, but the time window is extremely small.
 *
 * Since exit() isn't performance-critical, we wrap the call to
 * vperfctr_stub.exit() with code to increment the module's use
 * count before the call, and decrement it again afterwards. Thus,
 * the final drop to zero occurs here and not in the module itself.
 * (All other code paths that drop the use count do so via a file
 * object, and VFS in 2.4+ kernels also refcount the module.)
 */
void _vperfctr_exit(struct vperfctr *perfctr)
{
	__module_get(vperfctr_stub.owner);
	vperfctr_stub.exit(perfctr);
	module_put(vperfctr_stub.owner);
}

EXPORT_SYMBOL(vperfctr_stub);
EXPORT_SYMBOL___put_task_struct;

#if defined(CONFIG_UTRACE)
/* alas, I don't yet know how to convert this to utrace */
int ptrace_check_attach(struct task_struct *task, int kill) { return -ESRCH; }
#else
#include <linux/mm.h> /* for 2.4.15 and up, except 2.4.20-8-redhat */
#include <linux/ptrace.h> /* for 2.5.32 and up, and 2.4.20-8-redhat */
#endif
EXPORT_SYMBOL(ptrace_check_attach);
