/*************************************************************
 * File         : user-src/src/tests/timepapi.c
 * Version      : $Id: timepapi.c,v 1.1.2.1 2007/04/18 23:15:06 anataraj Exp $
 *
 ************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "papi.h"

#define MAX_NO_REPS 50000
#define DEFAULT_NOREPS 100
#define NO_OPS 32

#define MAX_NO_SYMS 10
#define DEFAULT_NOSYMS 5

//papi operations
#define PAPI_OP_NOOP		0
#define PAPI_OP_LIBINIT		1
#define PAPI_OP_THREADINIT	2
#define PAPI_OP_CREATE_EVSET	3
#define PAPI_OP_ADD_EVS		4
#define PAPI_OP_START		5
#define PAPI_OP_READ		6
#define PAPI_OP_STOP		7
#define MAX_PAPI_OP		8

char* papi_op_desc[MAX_PAPI_OP] = {
	"PAPI_OP_NOOP",
	"PAPI_OP_LIBINIT",
	"PAPI_OP_THREADINIT",
	"PAPI_OP_CREATE_EVSET",
	"PAPI_OP_ADD_EVS",
	"PAPI_OP_START",
	"PAPI_OP_READ",
	"PAPI_OP_STOP",
};

unsigned long long ktau_rdtsc(void);
double ktau_get_tsc(void);

#define SET_MIN_MAX(OP, TIME) { \
				if(timings_max[(OP)].time < (TIME).last) { \
					timings_max[(OP)].time = (TIME).last; \
				} \
				if(timings_min[(OP)].time > (TIME).last) { \
					timings_min[(OP)].time = (TIME).last; \
				} \
			}
		

#define start_timing(T) {(T).start = ktau_rdtsc();}
#define stop_timing(OP, T) {(T).last = (ktau_rdtsc() - (T).start); (T).time += (T).last; SET_MIN_MAX((OP), (T));}

typedef struct _timing_val {
	unsigned long long start;
	unsigned long long last;
	unsigned long long time;
} timing_val;

#define XSTR(sym) #sym
#define STR(sym) XSTR(sym)

timing_val timings[MAX_NO_REPS][MAX_PAPI_OP];
timing_val timings_avg[MAX_PAPI_OP];
timing_val timings_max[MAX_PAPI_OP];
timing_val timings_min[MAX_PAPI_OP];




int test_timing_papi(int no_reps, int max_nr_syms) {
	int EventSet = 0;
	//char ctr_name[KTAU_MAX_COUNTERNAME] = "PAPI_TEST_CTR01";
	int nr_cont = 0, result = 1, ret = 0, nr_syms = 0, i = 0;
	unsigned int sym[32];
	int pid = 0, rep = 0, rc = 0;
	unsigned long long excl[32];

	nr_syms = 0;
	//setup sym to point to some instrumentation point - need to automate this
	//PAPI_L1_TCM     0x80000006      Yes     No      Level 1 cache misses
	//PAPI_L2_TCM     0x80000007      Yes     No      Level 2 cache misses
	sym[nr_syms++] = 0x80000006; //PAPI_L1_TCM
	sym[nr_syms++] = 0x80000007; //PAPI_L2_TCM

	if(nr_syms < max_nr_syms) {
		printf("test_timing_shctr: max_nr_syms:%d is greater than available-nr_syms:%d. Setting max to avail.\n", max_nr_syms, nr_syms);
		max_nr_syms = nr_syms;
	}

	printf("FINAL CONFIG: No Reps: %d  Max Syms:%d\n", no_reps, max_nr_syms);

	//reset MIN values
	int op=0;
	for(op = 0;op< MAX_PAPI_OP; op++) {
		timings_min[op].time = 0xFFFFFFFFFFFFFFFF;
	}

	//rc = PAPI_event_name_to_code(name, &code);

	for(rep = 0; rep< no_reps; rep++) {
		//printf("Iteration:%d\n", rep);
		int thisrep = 0;

		start_timing(timings[thisrep][PAPI_OP_LIBINIT]);
		int papi_ver = PAPI_library_init(PAPI_VER_CURRENT);
		//int papi_ver = PAPI_library_init(50593792);
		stop_timing(PAPI_OP_LIBINIT, timings[thisrep][PAPI_OP_LIBINIT]);
		//printf("PAPI_library_init says: %d\n", papi_ver);


		start_timing(timings[thisrep][PAPI_OP_THREADINIT]);
		rc = PAPI_thread_init((unsigned long (*)(void))(NULL)); //no idea what this means, took from Tau
		stop_timing(PAPI_OP_THREADINIT, timings[thisrep][PAPI_OP_THREADINIT]);
		if (rc != PAPI_OK) {
			fprintf (stderr, "Error thread-init PAPI: %s\n", PAPI_strerror(rc));
			return -1;
		}
	
		EventSet = PAPI_NULL;
		start_timing(timings[thisrep][PAPI_OP_CREATE_EVSET]);
		rc = PAPI_create_eventset(&EventSet);
		stop_timing(PAPI_OP_CREATE_EVSET, timings[thisrep][PAPI_OP_CREATE_EVSET]);
		if (rc != PAPI_OK) {
			fprintf (stderr, "Error creating PAPI event set: %s\n", PAPI_strerror(rc));
			return -1;
		}

		start_timing(timings[thisrep][PAPI_OP_ADD_EVS]);
		rc = PAPI_add_events(EventSet, sym, max_nr_syms);
		stop_timing(PAPI_OP_ADD_EVS, timings[thisrep][PAPI_OP_ADD_EVS]);
		if (rc != PAPI_OK) {
			fprintf (stderr, "Error adding PAPI events: %s\n", PAPI_strerror(rc));
			return -1;
		}

		PAPI_shutdown();
	}

	int papi_ver = PAPI_library_init(PAPI_VER_CURRENT);
	//int papi_ver = PAPI_library_init(50593792);
	//printf("PAPI_library_init says: %d\n", papi_ver);

	rc = PAPI_thread_init((unsigned long (*)(void))(NULL)); //no idea what this means, took from Tau
	if (rc != PAPI_OK) {
		fprintf (stderr, "Error thread-init PAPI: %s\n", PAPI_strerror(rc));
		return -1;
	}

	EventSet = PAPI_NULL;
	rc = PAPI_create_eventset(&EventSet);
	if (rc != PAPI_OK) {
		fprintf (stderr, "Error creating PAPI event set: %s\n", PAPI_strerror(rc));
		return -1;
	}

	rc = PAPI_add_events(EventSet, sym, max_nr_syms);
	if (rc != PAPI_OK) {
		fprintf (stderr, "Error adding PAPI events: %s\n", PAPI_strerror(rc));
		return -1;
	}

	for(rep = 0; rep< no_reps; rep++) {
		//printf("Iteration:%d\n", rep);
		int thisrep = 0;

		start_timing(timings[thisrep][PAPI_OP_START]);
		rc = PAPI_start(EventSet);
		stop_timing(PAPI_OP_START, timings[thisrep][PAPI_OP_START]);
		if (rc != PAPI_OK) {
			fprintf (stderr, "Error starting PAPI: %s\n", PAPI_strerror(rc));
			return -1;
		}

		start_timing(timings[thisrep][PAPI_OP_READ]);
		rc = PAPI_read(EventSet, excl);
		stop_timing(PAPI_OP_READ, timings[thisrep][PAPI_OP_READ]);
		if (rc != PAPI_OK) {
			fprintf (stderr, "Error reading PAPI: %s\n", PAPI_strerror(rc));
			return -1;
		}

		start_timing(timings[thisrep][PAPI_OP_STOP]);
		rc = PAPI_stop(EventSet, excl);
		stop_timing(PAPI_OP_STOP, timings[thisrep][PAPI_OP_STOP]);
		if (rc != PAPI_OK) {
			fprintf (stderr, "Error stoping PAPI: %s\n", PAPI_strerror(rc));
			return -1;
		}

	}//for rep

	PAPI_shutdown();

	return 0;
}



void print_timings(int no_reps) {
	int rep = 0, op = 0;
	double tsc = ktau_get_tsc();
	for(op = 0; op<MAX_PAPI_OP; op++) {
		//init
		//timings_min[op].time = ~((unsigned long long)(0));
		printf("OP:%d %s:\n", op, papi_op_desc[op]);
		//for(rep = 0; rep<no_reps; rep++) {
		for(rep = 0; rep<1; rep++) {
			//printf("%d]. %llu\n", rep, timings[rep][op].time);
			timings_avg[op].time += timings[rep][op].time;
			/*
			if(timings_max[op].time < timings[rep][op].time) {
				timings_max[op].time = timings[rep][op].time;
			} else if(timings_min[op].time > timings[rep][op].time) {
				timings_min[op].time = timings[rep][op].time;
			}*/
		}
		timings_avg[op].time /= no_reps;
		printf("\t OP AVG:%llu (cycles)  %lf (micros)\n", timings_avg[op].time, (timings_avg[op].time/tsc)*1000000);
		printf("\t OP MAX:%llu (cycles)  %lf (micros)\n", timings_max[op].time, (timings_max[op].time/tsc)*1000000);
		printf("\t OP MIN:%llu (cycles)  %lf (micros)\n", timings_min[op].time, (timings_min[op].time/tsc)*1000000);
	}

	//time_the_timing();
	//time_GTOD();
	//time_slashproc();
}

int main(int argc, char* argv[]) {
	int no_reps = DEFAULT_NOREPS, i = 0, max_nr_syms = DEFAULT_NOSYMS;
	if(argc > 1) {
		no_reps = atoi(argv[1]);
	}
	if(no_reps <= 0 || no_reps >= MAX_NO_REPS) {
		printf("Bad no reps:%d, defaulting to:%d\n", no_reps, DEFAULT_NOREPS);
		no_reps = DEFAULT_NOREPS;
	}
	if(argc > 2) {
		max_nr_syms = atoi(argv[2]);
	}
	if(max_nr_syms <= 0 || max_nr_syms >= MAX_NO_SYMS) {
		printf("Bad no syms:%d, defaulting to:%d\n", max_nr_syms, DEFAULT_NOSYMS);
		no_reps = DEFAULT_NOREPS;
	}

	test_timing_papi(no_reps, max_nr_syms);

	print_timings(no_reps);

	printf("\n=========*****==========\n");
}

