#ifndef SYMS_H
#define SYMS_H

/* NOTE: this is a hacked up test directory - requires update according to environment *
 * Please put the correct symbol information here before using. This can be got from   *
 * /proc/kallsyms or from System.map from source tree. */

#define SYS_GETPID	0x8002ace4
#define SYS_NANOSLEEP	0x8003c1b0
#define SCHEDULE_VOL	0x80017228
#define SCHEDULE	0x8015edc0
#define __DO_IRQ	0x80045b48
#define DO_PAGE_FAULT	0x8000af40
#define ICMP_REPLY	0x8013bb84
#define __DO_SOFTIRQ	0x800262e8
#define SMP_CALL_FUNCTION_INT	0x80004f98
#define SMP_APIC_TIMER_INT	0x0
#define TIMER_INTERRUPT	0x80003f9c
#endif /* SYMS_H */

