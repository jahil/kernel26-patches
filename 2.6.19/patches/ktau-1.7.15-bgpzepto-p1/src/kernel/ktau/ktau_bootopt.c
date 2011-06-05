/******************************************************************************
 * File 	: kernel/ktau/ktau_bootopt.c
 * Version	: $Id: ktau_bootopt.c,v 1.2.2.1 2007/08/09 06:21:12 anataraj Exp $
 * ***************************************************************************/ 

/* kernel headers */
#include <linux/init.h>
//#include <linux/config.h> //breaks 2.6.19+
#include <linux/autoconf.h>
#include <linux/mm.h>
#include <linux/sched.h>

/* ktau headers */
#define TAU_NAME "ktau_bootopt" 
#include <linux/ktau/ktau_print.h>
#include <linux/ktau/ktau_bootopt.h>

/*
 * This is the bitmask parsed from the boot options
 */
unsigned int ktau_bootopt_bits  = 0;

/* 
 * Function	: ktau_bootopt_parse
 * Description	:
 * 	This function parse the kernel boot option to enable 
 * 	KTAU instrumentations.
 */
void ktau_bootopt_parse(char* cmdline){

#ifdef CONFIG_KTAU_BOOTOPT

	printk("ktau_bootopt_parse: cmdline = %s\n",cmdline);

#ifdef CONFIG_KTAU_SYSCALL
	if(strstr(cmdline, "ktau_syscall")){
		printk("ktau_bootopt_parse: enable ktau_syscall\n");
		ktau_bootopt_bits = ktau_bootopt_bits | KTAU_SYSCALL_MSK;
	}
#endif /*CONFIG_KTAU_SYSCALL*/

#ifdef CONFIG_KTAU_IRQ
	if(strstr(cmdline, "ktau_irq")){
		printk("ktau_bootopt_parse: enable ktau_irq\n");
		ktau_bootopt_bits = ktau_bootopt_bits | KTAU_IRQ_MSK;
	}
#endif /*CONFIG_KTAU_IRQ*/

#ifdef CONFIG_KTAU_BH
	if(strstr(cmdline, "ktau_bh")){
		printk("ktau_bootopt_parse: enable ktau_bh\n");
		ktau_bootopt_bits = ktau_bootopt_bits | KTAU_BH_MSK;
	}
#endif /*CONFIG_KTAU_BH*/

#ifdef CONFIG_KTAU_SCHED
	if(strstr(cmdline, "ktau_sched")){
		printk("ktau_bootopt_parse: enable ktau_sched\n");
		ktau_bootopt_bits = ktau_bootopt_bits | KTAU_SCHED_MSK;
	}
#endif /*CONFIG_KTAU_SCHED*/

#ifdef CONFIG_KTAU_EXCEPTION
	if(strstr(cmdline, "ktau_exception")){
		printk("ktau_bootopt_parse: enable ktau_exception\n");
		ktau_bootopt_bits = ktau_bootopt_bits | KTAU_EXCEPTION_MSK;
	}
#endif /*CONFIG_KTAU_EXCEPTION*/

#ifdef CONFIG_KTAU_SIGNAL
	if(strstr(cmdline, "ktau_signal")){
		printk("ktau_bootopt_parse: enable ktau_signal\n");
		ktau_bootopt_bits = ktau_bootopt_bits | KTAU_SIGNAL_MSK;
	}
#endif /*CONFIG_KTAU_SIGNAL*/

#ifdef CONFIG_KTAU_SOCKET
	if(strstr(cmdline, "ktau_socket")){
		printk("ktau_bootopt_parse: enable ktau_socket\n");
		ktau_bootopt_bits = ktau_bootopt_bits | KTAU_SOCKET_MSK;
	}
#endif /*CONFIG_KTAU_SOCKET*/

#ifdef CONFIG_KTAU_TCP
	if(strstr(cmdline, "ktau_tcp")){
		printk("ktau_bootopt_parse: enable ktau_tcp\n");
		ktau_bootopt_bits = ktau_bootopt_bits | KTAU_TCP_MSK;
	}
#endif /*CONFIG_KTAU_TCP*/

#ifdef CONFIG_KTAU_ICMP
	if(strstr(cmdline, "ktau_icmp")){
		printk("ktau_bootopt_parse: enable ktau_icmp\n");
		ktau_bootopt_bits = ktau_bootopt_bits | KTAU_ICMP_MSK;
	}
#endif /*CONFIG_KTAU_ICMP*/

#ifdef CONFIG_KTAU_TRACE
	if(strstr(cmdline, "ktau_trace")){
		printk("ktau_bootopt_parse: enable ktau_trace\n");
		ktau_bootopt_bits = ktau_bootopt_bits | KTAU_TRACE_MSK;
	}
#endif /*CONFIG_KTAU_TRACE*/

	//printk("ktau_bootopt_parse: ktau_bootopt_bits = %x\n",ktau_bootopt_bits);

#endif /*CONFIG_KTAU_BOOTOPT*/
}

/* 
 * Function	: ktau_verify_bootopt
 * Description	:
 * 	This function parse is called by the ktau_start_prof and ktau_stop_prof
 * 	to verify the mask given by the instrumentation with the bitmask parsed
 * 	from the kernel boot option at the initialize phase.
 */
/*
inline int ktau_verify_bootopt(unsigned int mask){

	if(unlikely(ktau_bootopt_bits & mask)){
		return(1);
	}else{
		return(0);
	}
}
*/
