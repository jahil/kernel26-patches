/*************************************************************
 * File         : user-src/src/TAU_trace.c
 * Version      : $Id: TAU_trace.c,v 1.6 2006/11/12 00:26:31 anataraj Exp $
 *
 ************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <fcntl.h>

#include "TAU_trace.h"

#define __trace_write(cur,_ev,_nid,_tid,_par,_ti) \
	(*cur)->ev 	= _ev;\
	(*cur)->nid	= _nid;\
	(*cur)->tid	= _tid;\
	(*cur)->par	= _par;\
	(*cur)->ti	= _ti

/* Function	: trace_init
 * Description	: Initialize the TAU trace output format
 */
int trace_init(t_ev** 		buffer,
		t_ev** 		cur, 
		x_int32 	ev,
		x_uint16        nid,   /* -- node id         -- */
		x_uint16        tid,   /* -- thread id       -- */
		x_int64         par,   /* -- event parameter -- */
		x_uint64        ti     /* -- time [us]       -- */
		)
{
	int retval = 0;

	/* We have to check if the first event is Entry type event 
	 * If it is FUNC_EXIT, then we discard this event
	 */
	if(par == FUNC_EXIT){
		return(-1);
	}	

	/* Allocate trace event buffer */
	*buffer = (t_ev*)malloc(sizeof(t_ev) * MAX_NUM_TRACE);
	*cur    = (t_ev*)*buffer;

	/* EV_INT must be the first event in the trace */
	__trace_write(cur,PCXX_EV_INIT,nid,0,3,ti);
	
	/* Advance the current pointer */
	*cur = (*cur)+1;

	/* WALL_CLOCK */
	//__trace_write(cur,PCXX_EV_WALL_CLOCK,0,0,ti/1000000,ti);

	/* Advance the current pointer */
	//*cur = (*cur)+1;

	/* Real first trace event */
	__trace_write(cur,ev,nid,tid,par,ti);

	return retval;
}

//x_uint64 last_ti = 0;

/* Function	: trace_write
 * Description	: TAU trace output writer
 */
int trace_write(t_ev**		buf,
		t_ev** 		cur, 
		x_uint64*	last_ti, 
		x_int32 	ev,
		x_uint16        nid,   /* -- node id         -- */
		x_uint16        tid,   /* -- thread id       -- */
		x_int64         par,   /* -- event parameter -- */
		x_uint64        ti,    /* -- time [us]       -- */
		char 		*output_path)
{
	static int init_done =0;
	int retval = 0;
	
	/* Check if the buffer is already initialized */
	if(*buf == NULL && *cur== NULL){
		if((retval = trace_init(buf,cur,ev,nid,tid,par,ti))){
			return retval;
		}
	}else{
		/* 
		 * cur is pointing to the last trace event.
		 * We have to advance cur to the next available 
		 * space and check for out-of-bound.
		 */
		if( (*cur)-(*buf) >= MAX_NUM_TRACE-3 ){
			trace_dump_cont(buf, cur, output_path);
		}else{
			/* Advance the current pointer */
			*cur = (*cur)+1;
		}	
		/* Write the trace event */
		__trace_write(cur,ev,nid,tid,par,ti);
	}
	*last_ti = ti;	
	//printf("DEBUG: trace_write: ev= %d, retval = %d\n", ev,retval);
	return retval;
}

/* Function	: trace_end
 * Description	: Finalize the TAU trace output format
 */
int trace_end(t_ev ** buffer, t_ev ** cur, x_uint64 *last_ti, x_uint16 nid){
	int retval = 0;

	*cur = *(cur)+1;
	/* FLUSH_CLOSE */
	retval = __trace_write(cur,PCXX_EV_CLOSE,nid,0,0,*last_ti);
	*cur = *(cur)+1;
	/* WALL_CLOCK must be the last event in the trace */
	retval = __trace_write(cur,PCXX_EV_WALL_CLOCK,nid,0,(*last_ti) * 1e-6,*last_ti);
	
	return retval; 	
}

/* Function	: register_ev
 * Description	: Registering function for trace event
 */
int register_ev(t_id** buf, 
		unsigned int addr,
		char* group,
		int tag,
		char* name_type,
		char* param)
{
	static int id_buf_init = 0;
	static int cur_fid = 1;
	int i;

	/* Check for initialization */
	if(!id_buf_init){
		*buf = malloc(sizeof(t_id) * MAX_TRACE_ID);
		memset(*buf, 0, sizeof(t_id) * MAX_TRACE_ID);
		id_buf_init = 1;
		
		/* Register the first event */
		(*buf)->addr		= addr;
		(*buf)->fid		= cur_fid;
		memcpy((*buf)->group, group, MAX_CHAR_LEN);
		(*buf)->tag		= tag;
		memcpy((*buf)->name_type, name_type, MAX_CHAR_LEN);
		memcpy((*buf)->param, param, MAX_CHAR_LEN);
		cur_fid++;
		
		return(cur_fid-1);
	}

	/* Search the function id list */
	for(i=0;i<cur_fid;i++){
		if((*buf+i)->addr == addr){	
			return (*buf+i)->fid;
		}
	}

	if(cur_fid <= MAX_TRACE_ID){	
		/* Address is not found, so assign the next id */
		(*buf+cur_fid-1)->addr		= addr;
		(*buf+cur_fid-1)->fid		= cur_fid;
		memcpy((*buf+cur_fid-1)->group, group, MAX_CHAR_LEN);
		(*buf+cur_fid-1)->tag		= tag;
		memcpy((*buf+cur_fid-1)->name_type, name_type, MAX_CHAR_LEN);
		memcpy((*buf+cur_fid-1)->param, param, MAX_CHAR_LEN);
		cur_fid++;
		
		//printf("DEBUG: cur_fid = %d, register_ev => addr= %x, group= %s, name_type= %s\n",
		//			(cur_fid-1), addr, group, name_type); 	
		
		return(cur_fid-1);
	}else{
		/* Function ID exceed maximum*/
		printf("ERROR: Function ID exceed maximum.\n");
		return(-1);	
	}
}

/* Function	: edf_dump
 * Description	: Dumping edf file containing function ID
 */
int edf_dump(t_id** buf, char* output_path){
	int num_t_id = 9;
	int i;	
	FILE *fd;
	char output_path_edf[MAX_CHAR_LEN];

	sprintf(output_path_edf,"%s.edf",output_path);
	if((fd = fopen(output_path_edf,"w+")) == NULL ){
		perror("fopen");
		return(-1);
	}
	
	/* Count total function ID */
	for(i=0 ; i<MAX_TRACE_ID ; i++){
		if((*buf+i)->fid)
			num_t_id++;	
	}

	/* Header of edf file */
	fprintf(fd,"%d dynamic_trace_events\n",num_t_id);
	fprintf(fd,"# FunctionId Group Tag \"Name Type\" Parameters\n");

	/* Real trace events */
	for(i=0 ; i<MAX_TRACE_ID ; i++){
		if((*buf+i)->fid){
			fprintf(fd,"%u %s %d \"%s\" %s\n",
				(*buf+i)->fid,
				(*buf+i)->group,
				(*buf+i)->tag,
				(*buf+i)->name_type,
				(*buf+i)->param);
		}
	}
	free(*buf);		

        // Now add the nine extra events 
        fprintf(fd,"%ld TRACER 0 \"EV_INIT\" none\n", (long) PCXX_EV_INIT);
        fprintf(fd,"%ld TRACER 0 \"FLUSH_ENTER\" none\n", (long) PCXX_EV_FLUSH_ENTER);
        fprintf(fd,"%ld TRACER 0 \"FLUSH_EXIT\" none\n", (long) PCXX_EV_FLUSH_EXIT);
        fprintf(fd,"%ld TRACER 0 \"FLUSH_CLOSE\" none\n", (long) PCXX_EV_CLOSE);
        fprintf(fd,"%ld TRACER 0 \"FLUSH_INITM\" none\n", (long) PCXX_EV_INITM);
        fprintf(fd,"%ld TRACER 0 \"WALL_CLOCK\" none\n", (long) PCXX_EV_WALL_CLOCK);
        fprintf(fd,"%ld TRACER 0 \"CONT_EVENT\" none\n", (long) PCXX_EV_CONT_EVENT);
        fprintf(fd,"%ld TAU_MESSAGE -7 \"MESSAGE_SEND\" par\n", (long) TAU_MESSAGE_SEND);
        fprintf(fd,"%ld TAU_MESSAGE -8 \"MESSAGE_RECV\" par\n", (long) TAU_MESSAGE_RECV);
	fclose(fd);
	return 0;	
}

/* Function	: trace_dump_cont
 * Description	: Dumping trace file in TAU output format without 
 * 		  freeing the infrastructure.
 */
int trace_dump_cont(t_ev** buffer, t_ev** cur, char* output_path){
	int retval = 0;
	int dumpsize = (*cur - *buffer) + 1;
	FILE *fd;

	if((fd = fopen(output_path,"a")) == NULL ){
		perror("fopen");
		return(-1);
	}
	
	//printf("DEBUG: dumpsize = %d\n",dumpsize);
		
	retval = fwrite(*buffer,1,dumpsize * sizeof(t_ev),fd);

	*cur 	= *buffer;
	
	fclose(fd);
	return retval;
}

/* Function	: trace_dump
 * Description	: Dumping trace file in TAU output format
 */
int trace_dump(t_ev** buffer, t_ev** cur, char* output_path){
	int retval = 0;
	int dumpsize = (*cur - *buffer) + 1;
	FILE *fd;

	if((fd = fopen(output_path,"a+")) == NULL ){
		perror("fopen");
		return(-1);
	}
	
	//printf("DEBUG: dumpsize = %d\n",dumpsize);
		
	retval = fwrite(*buffer,1,dumpsize * sizeof(t_ev),fd);

	free(*buffer);
	*buffer = NULL;
	*cur 	= NULL;
	
	fclose(fd);
	return retval;
}
