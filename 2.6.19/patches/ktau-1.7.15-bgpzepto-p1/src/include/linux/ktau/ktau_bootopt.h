/*************************************************************************
* File 		: include/linux/ktau/ktau_bootopt.h
* Version	: $Id: ktau_bootopt.h,v 1.2 2006/11/12 00:26:28 anataraj Exp $
*************************************************************************/

#ifndef _KTAU_BOOTOPT_H_
#define _KTAU_BOOTOPT_H_

/*
 * Bit Masks for each group of KTAU instrumentation
 */
#define KTAU_SYSCALL_MSK	1
#define KTAU_IRQ_MSK		2
#define KTAU_BH_MSK		4
#define KTAU_SCHED_MSK		8
#define KTAU_EXCEPTION_MSK	16
#define KTAU_SIGNAL_MSK		32
#define KTAU_SOCKET_MSK		64	
#define KTAU_TCP_MSK		128
#define KTAU_ICMP_MSK		256	



#define KTAU_TRACE_MSK		32768

/*
 * This is the bitmask parsed from the boot options
 */
extern unsigned int ktau_bootopt_bits;


static inline int ktau_verify_bootopt(unsigned int grp);

/* 
 * Function     : ktau_verify_bootopt
 * Description  :
 *      This function parse is called by the ktau_start_prof and ktau_stop_prof
 *      to verify the mask given by the instrumentation with the bitmask parsed
 *      from the kernel boot option at the initialize phase.
 */
static inline int ktau_verify_bootopt(unsigned int mask){

        if(unlikely(ktau_bootopt_bits & mask)){
                return(1);
        }else{
                return(0);
        }
}

#endif /* _KTAU_BOOTOPT_H_ */
