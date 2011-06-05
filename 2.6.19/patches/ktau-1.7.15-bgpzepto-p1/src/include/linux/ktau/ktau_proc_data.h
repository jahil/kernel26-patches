/***********************************************
 * File		: include/linux/ktau/ktau_proc_data.h
 * Version	: $Id: ktau_proc_data.h,v 1.5.2.6 2007/04/12 04:18:23 anataraj Exp $
 ***********************************************/

#ifndef _KTAU_PROC_DATA_H
#define _KTAU_PROC_DATA_H

#include <linux/ktau/ktau_datatype.h>
#include <linux/ktau/ktau_inject.h>

/* User-Space <----> ktau-proc query/data-exchange Format *
 *--------------------------------------------------------*/

/* Output Flags - that ktau returns */
#define KTAU_SUCCESS		(1 << 0) /* all is well */
#define KTAU_TOLERANCE_PASS	(1 << 1) /* tolerance was fine, so no data was passed back */


/* KTAU Proc's MAGIC NO to identify correct calls *
 *------------------------------------------------*/
#define KTAU_PROC_MAGIC			0xbadbeef
#define KTAU_PROC_MAGIC_NOOP		0xbaabaaa

/* Type of information (Trace/Profile/etc) *
 *-----------------------------------------*/
#define KTAU_TYPE_PROFILE	0x0
#define KTAU_TYPE_TRACE		0x1
/* ...PLACE ANY NEW TYPES ABOVE THIS... (bounds checking) */
#define KTAU_MAX_TYPE		0xF

/* Sub-Type of information - 	    *
 * for extensibility - not yet used *
 *----------------------------------*/
#define KTAU_MAX_SUBTYPE	0xF

/* Cmd being processed (request size/request data/etc) *
 *-----------------------------------------------------*/
#define KTAU_CMD_SIZE		0x0
#define KTAU_CMD_READ		0x1
#define KTAU_CMD_MERGE		0x2
#define KTAU_CMD_RESIZE		0x3
#define KTAU_CMD_ADDSHCONT	0x4
#define KTAU_CMD_DELSHCONT	0x5
#define KTAU_CMD_ADDSHCTR	0x6
#define KTAU_CMD_DELSHCTR	0x7
#define KTAU_CMD_INJECT		0x8
/* ...PLACE ANY NEW CMDS ABOVE THIS... (bounds checking) */
#define KTAU_MAX_CMD		0xF

/* Sub-Cmd field - for extensibility - not yet used *
 *--------------------------------------------------*/
#define KTAU_MAX_SUBCMD		0xF

/* Format of the Bit-Vector holding Type/Subtype/Cmd/SubCmd info:  *
 * |<8 bits Type> <8 bits Sub-Type> <8 bits Cmd> <8 bits Sub-Cmd>| *
 * i.e a u32. */
/* Offsets for the different parameters */
#define KTAU_TYPE_OFFSET	24
#define KTAU_SUBTYPE_OFFSET	16
#define KTAU_CMD_OFFSET		8
#define KTAU_SUBCMD_OFFSET	0

/* Taregt of query (own profile/profile of all/many/etc) *
 *-------------------------------------------------------*/
#define KTAU_TARGET_SELF	0x0
#define KTAU_TARGET_PID		0x1
#define KTAU_TARGET_MANY	0x2
#define KTAU_TARGET_ALL		0x3
/* ...PLACE ANY NEW TARGETS ABOVE THIS... (bounds checking) */
#define KTAU_MAX_TARGET		0xF


/* Setting & Getting Macros for the Bit-Flag *
 *--------------------------------------------*/
/* Generic Set Macro */
#define KTAU_SET_FIELD(BITFLAG, VAL, OFFSET)	( (BITFLAG) = (BITFLAG) | ( (VAL) << (OFFSET) ) )
/* Field-Specific Set Macros */
#define KTAU_SET_TYPE(BITFLAG, VAL)	KTAU_SET_FIELD(BITFLAG, VAL, KTAU_TYPE_OFFSET)
#define KTAU_SET_SUBTYPE(BITFLAG, VAL)	KTAU_SET_FIELD(BITFLAG, VAL, KTAU_SUBTYPE_OFFSET)
#define KTAU_SET_CMD(BITFLAG, VAL)	KTAU_SET_FIELD(BITFLAG, VAL, KTAU_CMD_OFFSET)
#define KTAU_SET_SUBCMD(BITFLAG, VAL)	KTAU_SET_FIELD(BITFLAG, VAL, KTAU_SUBCMD_OFFSET)

/* Generic Get Macro */
#define KTAU_GET_FIELD(BITFLAG, OFFSET)	( (BITFLAG >> OFFSET) & (0xF) )
/* Field-Specific Set Macros */
#define KTAU_GET_TYPE(BITFLAG)		KTAU_GET_FIELD(BITFLAG, KTAU_TYPE_OFFSET)
#define KTAU_GET_SUBTYPE(BITFLAG)	KTAU_GET_FIELD(BITFLAG, KTAU_SUBTYPE_OFFSET)
#define KTAU_GET_CMD(BITFLAG)		KTAU_GET_FIELD(BITFLAG, KTAU_CMD_OFFSET)
#define KTAU_GET_SUBCMD(BITFLAG)	KTAU_GET_FIELD(BITFLAG, KTAU_SUBCMD_OFFSET)


/* Q: How do we make sure NO    *
 * padding is added to below structures *
 * ?
 */

/* Data types *
 *------------*/
typedef struct _ktau_tolerance {
	unsigned long tol_in;		/* in-arg: tolerance for duplicate reads */
	unsigned long time;		/* out-arg: last read when */
} ktau_tolerance;


/* Structs for Specific Operartions/Cmds *
 *---------------------------------------*/
typedef struct _ktau_size {
	unsigned long size;		/* out-arg: size of profile */	
} ktau_size;

typedef struct _ktau_read {
	unsigned long size;		/* in-arg: size of data buffer */	
	char* buf;			/* out-arg: profile data */
} ktau_read;

typedef struct _ktau_merge {
	//ktau_state** ppstate;		/* in-arg: user-space state */	
	ktau_state* ppstate;		/* in-arg: user-space state TODO: Change to pstate */
} ktau_merge;

typedef struct _ktau_resize {
	unsigned long size;		/* out-arg: new size of trace (only) */	
} ktau_resize;

typedef struct _ktau_share {
	ktau_ushcont ushcont;		/* in-arg: user-space (to be)shared container */
} ktau_share;

typedef struct _ktau_ctr {
	int cont_id; 			/* id of container req */
	ktau_ush_ctr ush_ctr;
} ktau_counter;

typedef struct _ktau_inject {
	int num;
	unsigned long long val[KTAU_MAX_INJECT_VALS];
} ktau_inject;


/* Union of all operations *
 *-------------------------*/
typedef union _ktau_op_data {
	ktau_size size;
	ktau_read read;
	ktau_merge merge;
	ktau_resize resize;
	ktau_share share;
	ktau_counter ctr;
	ktau_inject inject;
} ktau_op_data;


/* Struct for flags  & tolerance *
 *-------------------------------*/
typedef struct _ktau_query_flags {
	unsigned int mask;
	ktau_tolerance tol;
} ktau_query_flags;


/* struct defining the main query format used to talk to /proc	*
 * thro ioctl.							*
 *--------------------------------------------------------------*/
typedef struct _ktau_query {
	/* primary query attributes */
	u32		bitflag;	/* type/subtype/cmd/subcmd */
	u8		target;		/* self/many/all/?? */
	
	/* pid information */
	pid_t* 		pidlist;	/*list of pids interested in */ 
	int 		nopids;		/* no pids in the list */

	/* additional flags */
	ktau_query_flags	flags;		/* flags for memory mgmt etc */

	/* operation (or cmd) specific data */
	ktau_op_data 	op;		/* operation specifics */

	/* simple return value */
	int status;
} ktau_query;

#endif  /*_KTAU_PROC_DATA_H */

