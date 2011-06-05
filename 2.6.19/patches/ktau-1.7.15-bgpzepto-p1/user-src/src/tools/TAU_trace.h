/*************************************************************
 * File         : user-src/src/TAU_trace.h
 * Version      : $Id: TAU_trace.h,v 1.5 2006/11/12 00:26:31 anataraj Exp $
 *
 ************************************************************/

#ifndef __TAU_TRACE_H__
#define __TAU_TRACE_H__


/* 
 * This header file is partially copied from
 * tau2/include/Profile/pcxx_event.h
 */

#define MAX_NUM_TRACE		1024
#define MAX_TRACE_ID		1024
#define MAX_CHAR_LEN		100

/* -- pcxx tracer events ------------------- */
#define PCXX_EV_INIT         	60000
#define PCXX_EV_FLUSH_ENTER  	60001
#define PCXX_EV_FLUSH_EXIT   	60002
#define PCXX_EV_CLOSE        	60003
#define PCXX_EV_INITM        	60004
#define PCXX_EV_WALL_CLOCK   	60005
#define PCXX_EV_CONT_EVENT   	60006
#define TAU_MESSAGE_SEND     	60007
#define TAU_MESSAGE_RECV     	60008

#define FUNC_ENTER 		1
#define FUNC_EXIT		-1

typedef char x_int8;
typedef short x_int16;
typedef int x_int32;
typedef long long x_int64;

typedef unsigned char x_uint8;
typedef unsigned short x_uint16;
typedef unsigned int x_uint32;
typedef unsigned long long x_uint64;

/* -- event record buffer descriptor ------------------------- */
typedef struct
{
	x_int32		ev;    /* -- event id        -- */
	x_uint16	nid;   /* -- node id         -- */
	x_uint16	tid;   /* -- thread id       -- */
	x_int64		par;   /* -- event parameter -- */
	x_uint64	ti;    /* -- time [us]       -- */
} t_ev;

typedef struct
{
	unsigned int 	addr;
	unsigned int 	fid;
	char		group[MAX_CHAR_LEN];
	int		tag;
	char		name_type[MAX_CHAR_LEN];
	char		param[MAX_CHAR_LEN];
} t_id;

extern int trace_write(t_ev** buf, 
			t_ev** cur, 
			x_uint64 *last_ti,
			x_int32 ev, 
			x_uint16 nid,
                	x_uint16 tid, 
			x_int64 par, 
			x_uint64 ti, 
			char* output_path);

extern int trace_end(t_ev ** buffer, 
			t_ev ** cur,
			x_uint64 *last_ti,
			x_uint16 nid);

extern int register_ev(t_id** buf, 
		unsigned int addr, 
		char* group, 
		int tag,
                char* name_type, 
		char* param);

extern int edf_dump(t_id** buf, 
			char* output_path);

extern int trace_dump(t_ev** buffer, 
			t_ev** cur, 
			char* output_path);

extern int trace_dump_cont(t_ev** buffer, 
			t_ev** cur, 
			char* output_path);

#endif/* __TAU_TRACE_H__ */
