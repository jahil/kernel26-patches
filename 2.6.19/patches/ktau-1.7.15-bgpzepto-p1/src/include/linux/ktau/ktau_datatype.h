/***********************************************
 * File 	: include/linux/ktau/ktau_datatype.h
 * Version	: $Id: ktau_datatype.h,v 1.7.2.2 2007/08/09 06:21:12 anataraj Exp $
 * ********************************************/
#ifndef _KTAU_DATATYPE_H_
#define _KTAU_DATATYPE_H_

#ifndef KTAU_USER_SRC_COMPILE
//#include <linux/config.h> //breaks 2.6.19+
#include <linux/autoconf.h>
#endif /*KTAU_USER_SRC_COMPILE*/

#define KTAU_TIMER      0
#define KTAU_EVENT      1

#define KTAU_TRACE_START	0
#define KTAU_TRACE_STOP		1	

#ifndef CONFIG_KTAU_TRACE_MAX_IN_K
#define KTAU_TRACE_MAX          (5*1024)
#else /*CONFIG_KTAU_TRACE_MAX_IN_K*/
#define KTAU_TRACE_MAX		(CONFIG_KTAU_TRACE_MAX_IN_K*1024)
#endif /*CONFIG_KTAU_TRACE_MAX_IN_K*/
   
#define KTAU_TRACE_LAST         (KTAU_TRACE_MAX-1)

#include <linux/ktau/ktau_merge.h> /* User/Kernel Merge Functionality */
#include <linux/ktau/ktau_cont_data.h> /* User/Kernel Shared Containers */

/*
 * This struct is timer data type
 */
typedef struct _ktau_timer{
        unsigned int 		count;
        unsigned long long 	start;
        unsigned long long 	incl;
        unsigned long long 	excl;
} ktau_timer;

/*
 * This struct is event data type
 */
typedef struct _ktau_event{
        unsigned int 		count;
} ktau_event;

#ifdef CONFIG_KTAU_PERF_COUNTER

#define KTAU_MAX_PERF_COUNTER	16
extern unsigned int ktau_pcounter_mask;
extern unsigned int ktau_num_pcounter;

/*
 * This struct is counter data type
 */
typedef struct _ktau_perf_counter{
        unsigned int 		count;
        unsigned int 		start;
        unsigned int 		incl;
        unsigned int 		excl;
} ktau_perf_counter;
#endif /*CONFIG_KTAU_PERF_COUNTER*/

/*------------------------------------*/

/*
 * This struct is trace entry 
 */
typedef struct _ktau_trace {
        unsigned long long      tsc;
        unsigned int            addr;
        unsigned int            type;
}ktau_trace;

/*
 * This struct is used for each data type
 */
typedef union _ktau_data{
        ktau_timer 		timer;
        ktau_event 		event;
} ktau_data;

/*------------------------------------*/

/*
 * This struct is used in each entry of the
 * hash table
 */
typedef struct _h_ent{
        unsigned int 		addr;
	unsigned int 		type;
        ktau_data 		data;
        unsigned int 		nestingLevel;
#ifdef CONFIG_KTAU_PERF_COUNTER
        ktau_perf_counter 	pcounter[KTAU_MAX_PERF_COUNTER];
#endif /*CONFIG_KTAU_PERF_COUNTER*/
} h_ent;

/*
 * This struct is used to wrap over each 
 * hash table entry when output to ktau_proc 
 */
typedef struct _o_ent{
        unsigned int 		index;
	h_ent 			entry;	
} o_ent;

/*
 * This struct is the output to ktau_proc */
typedef struct _ktau_output{
	unsigned int 		size;
 	pid_t 			pid;
	o_ent 			*ent_lst;
	ktau_trace 		*trace_lst;
} ktau_output;

/*------------------------------------*/


#endif /*_KTAU_DATATYPE_H_*/
