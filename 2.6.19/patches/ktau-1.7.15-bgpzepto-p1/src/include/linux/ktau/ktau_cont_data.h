/******************************************************************************
 * Version	: $Id: ktau_cont_data.h,v 1.1.2.11 2008/11/19 05:20:47 anataraj Exp $
 * ***************************************************************************/ 

#ifndef _KTAU_CONT_DATA_H
#define _KTAU_CONT_DATA_H

typedef struct _ktau_dbl_buf {
	int cur_index;
	//char* buf[2]; two buffers following dbl_buf struct are implicit
} volatile ktau_dbl_buf;

/* ktau_dblbuf_size(size):
 * Sizeof the dbl_buf struct
 * plus the two buffers. */
#define ktau_dblbuf_size(size) (sizeof(ktau_dbl_buf) + ((size)*2))

/* The maximum number of shared containers *
 * per process.				   *
 * Currently - 1. TODO: CHANGE		   */
#define KTAU_MAX_SHCONTS 1


/* We have two representations of the Shared	*
 * Container. One (ktau_shcont) for the kernel	*
 * and two (ktau_ushcont) for the user-land.	*
 * Though some fields are repeated, this allows	*
 * keeping kernel control information hidden 	*
 * from user-land. 				*
 * -------------------------------------------- */

#ifdef __KERNEL__
/* Shared Container: Shared between OS *
 * and user-space of same process      */
typedef struct _ktau_shcont {
	struct list_head list;
	struct list_head free;
	char on;
	int kid;		/* kernel id */
	/* address of this container	*
	 * in user & kernel modes.	*/
	unsigned long uaddr, kaddr, orig_kaddr;
	struct page** pages;	/* for maping user buf */
	int nr_pages;		/* for maping user buf */
	unsigned long size;	/* size of each buffer below 	*/
	unsigned long next_off;	/* offset of next avail		*
				 * space in buffer.		*/
	int flags;		/* flags for extensibility */
	int ref_cnt;		/* reference count */
	int zcb_on;		/* zepto compute class binary */
	volatile ktau_dbl_buf* volatile buf;	/* the shared double buffer */
} ktau_shcont;
#endif /* __KERNEL__ */

/* Uspace Shared Container: Shared between OS *
 * and user-space of same process      */
typedef struct _ktau_ushcont {
	int kid;		/* kernel id */
	unsigned long size;	/* size of each buffer below 	*/
	unsigned long next_off;	/* offset of next avail		*
				 * space in buffer.		*/
	int flags;		/* flags for extensibility */
	int ref_cnt;		/* reference count */
	volatile ktau_dbl_buf*volatile buf;	/* the shared double buffer */
} ktau_ushcont;

/* ktau_ushcont_size(p_ushcont):
 * Sizeof the total shared user container,
 * including the container header
 * and size of the dbl_buf. */
#define ktau_ushcont_size(p_ushcont) (sizeof(ktau_ushcont) + ktau_dblbuf_size(p_ushcont->size))

/* To easily access either of the buffers in the double-buffer */
#define ktau_shcont2dblbuf(__PSHCONT, __INDEX)  ( (char*)(((unsigned long)((__PSHCONT)->buf)) + sizeof(ktau_dbl_buf) + ((__INDEX)*(__PSHCONT)->size)) )

/* Counters (esp. shared ones) */
#define KTAU_MAX_COUNTERNAME 32
#define KTAU_MAX_COUNTERSYMS 8
typedef struct _ktau_ctr_desc {
	unsigned long kid;
	char name[KTAU_MAX_COUNTERNAME];
	int no_syms;
	unsigned long syms[KTAU_MAX_COUNTERSYMS];
} ktau_ctr_desc;

#ifdef __KERNEL__
struct _ktau_trigger; //fwd decl

typedef struct _ktau_sh_ctr {
	struct list_head list;
	int type;
	ktau_ctr_desc desc;
	ktau_shcont* cont;
	unsigned long offset; /* offset within the container */
	struct _ktau_trigger* triggers;
} ktau_sh_ctr;
#endif /* __KERNEL__ */

typedef struct _ktau_ush_ctr {
	int type;
	ktau_ctr_desc desc;
	ktau_ushcont* ucont;
	unsigned long offset; /* offset within the container */
} ktau_ush_ctr;

#ifdef __KERNEL__
#define ktau_shctr2ctrdata(__PSHCTR, __INDEX) ( (ktau_data*)((unsigned long)ktau_shcont2dblbuf((__PSHCTR)->cont, __INDEX) + (__PSHCTR)->offset) )
#else
#define ktau_shctr2ctrdata(__PSHCTR, __INDEX) ( (ktau_data*)((unsigned long)ktau_shcont2dblbuf((__PSHCTR)->ucont, __INDEX) + (__PSHCTR)->offset) )
#endif /* __KERNEL */

#define KTAU_COUNTER_NONE 0x0
#define KTAU_COUNTER_SHARED 0x1

/* ktau_shctr_data_size(ktau_sh_ctr or ktau_ush_ctr):
 * Sizeof the data pointed to by ctr.
 * Does include the size of the ktau_sh_ctr struct itself.
 */
#define ktau_shctr_data_size(shctr) (sizeof(ktau_data) * ((shctr)->desc.no_syms))

#ifdef __KERNEL__
#define KTAU_TRIGGER_TYPE_NONE	0x0
#define KTAU_TRIGGER_TYPE_START	0x1
#define KTAU_TRIGGER_TYPE_STOP	0x2

typedef int (*KTAU_EVHDR_CTR)(struct _ktau_trigger*, int type, unsigned long long incl, unsigned long long excl);
/* Event-consumer registration */
typedef struct _ktau_trigger {
	struct list_head list;
	unsigned long sym; //the symbol (/addr) of interest
	int index; //identifies which trigger (i.e. which symbol's routine) fired
	int hash_index; //identifies which ktau_hash entry is being watched
	ktau_sh_ctr* ctr; //the counter
	KTAU_EVHDR_CTR handler; //the handler to be called on trigger fire
	char pending; //boolean - says which list trigger is on
} ktau_trigger;

#endif /* __KERNEL__ */

#endif /* _KTAU_CONT_DATA_H */
