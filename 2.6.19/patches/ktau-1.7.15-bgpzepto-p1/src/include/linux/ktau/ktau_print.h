/*************************************
 * File		: ktau_print.h
 * Version	: $Id: ktau_print.h,v 1.7.2.1 2008/11/19 05:20:47 anataraj Exp $
 *
 ************************************/
#ifndef _KTAU_PRINT_H_
#define _KTAU_PRINT_H_

#define PFX TAU_NAME

#ifndef TAU_DEBUG_COND
#define TAU_DEBUG_COND (1)
#endif //TAU_DEBUG_COND

#ifdef TAU_DEBUG
#define dbg(format, arg...)	do { if((TAU_DEBUG_COND)) printk(KERN_DEBUG PFX ": " format "\n" , ## arg);} while(0)
#define info(format, arg...) 	do { if((TAU_DEBUG_COND)) printk(KERN_INFO PFX ": " format "\n" , ## arg);} while(0)
#else /*TAU_DEBUG*/
#define dbg(format, arg...)  	do {} while (0)
#define info(format, arg...)  	do {} while (0)
#endif /*TAU_DEBUG*/

#define err(format, arg...)  	printk(KERN_ERR PFX ": " format "\n" , ## arg)
#define warn(format, arg...) 	printk(KERN_WARNING PFX ": " format "\n" , ## arg)

#endif /*_KTAU_PRINT_H_*/
