/*************************************************************
 * File         : user-src/src/tests/timing.c
 * Version      : $Id: timing.c,v 1.1.2.4 2007/04/12 04:18:23 anataraj Exp $
 *
 ************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "ktau_proc_interface.h"
#include "syms.h"

/* Test the ktau_get_counter, ktau_put_counter, ktau_copy_counter  interfaces */
#define SHCONT_SIZE 4096

#define MAX_NO_REPS 50000
#define DEFAULT_NOREPS 100
#define NO_OPS 32

#define MAX_NO_SYMS 10
#define DEFAULT_NOSYMS 5

//operations
#define OP_NOOP		0
#define OP_SHCONT_ADD	1
#define OP_SHCONT_DEL	2
#define OP_SHCTR_ADD	3
#define OP_SHCTR_DEL	4
#define OP_SHCTR_COPY	5
#define OP_SHCTR_CPEXCL	6
#define OP_SHCTR_PRINT	7
#define MAX_OP		8

char* op_desc[MAX_OP] = {
	"OP_NOOP",
	"OP_SHCONT_ADD",
	"OP_SHCONT_DEL",
	"OP_SHCTR_ADD",
	"OP_SHCTR_DEL",
	"OP_SHCTR_COPY",
	"OP_SHCTR_COPY_EXCL",
	"OP_SHCTR_PRINT"
};

unsigned long long ktau_rdtsc(void);
double ktau_get_tsc(void);

#define start_timing(T) {(T).start = ktau_rdtsc();}
#define stop_timing(T) {(T).time += (ktau_rdtsc() - (T).start);}

typedef struct _timing_val {
	unsigned long long start;
	unsigned long long time;
} timing_val;

unsigned long long time_the_timing() {
        volatile timing_val t_start, t_stop, dummy, t_min_start, t_min_stop;
        volatile int i = 1000, j = 0, k=100;
        double TSC = ktau_get_tsc();

	memset(&t_min_start, 0, sizeof(t_min_start));
	memset(&t_min_stop, 0, sizeof(t_min_stop));

	//set these to large values on startup
	stop_timing(t_min_start);
	stop_timing(t_min_stop);

	while(k--) {
		memset(&t_start, 0, sizeof(t_start));
		memset(&t_stop, 0, sizeof(t_stop));


		j=i;
		start_timing(t_start);
		while(j--) {
			start_timing(dummy);
		}
		stop_timing(t_start);
	
		
	
		j=i;
		start_timing(t_stop);
		while(j--) {
			stop_timing(dummy);
		}
		stop_timing(t_stop);

		if(t_min_start.time > t_start.time) {
			t_min_start.time = t_start.time;
		}
		if(t_min_stop.time > t_stop.time) {
			t_min_stop.time = t_stop.time;
		}


	}

        printf("time_the_timing:\n\tStart: %llu (cycles)  %lf (micros)", t_min_start.time/i, (t_min_start.time/TSC)*1000000/i);
        printf("\n\tStop: %llu (cycles)  %lf (micros)", t_min_stop.time/i, (t_stop.time/TSC)*1000000/i);

        return dummy.time;
}

unsigned long long time_GTOD() {
        volatile timing_val t_start, t_stop, dummy, t_min_start, t_min_stop;
        volatile int i = 1000, j = 0, k=100;
        double TSC = ktau_get_tsc();
	struct timeval tp;

	memset(&t_min_start, 0, sizeof(t_min_start));
	memset(&t_min_stop, 0, sizeof(t_min_stop));

	//set these to large values on startup
	stop_timing(t_min_start);
	stop_timing(t_min_stop);

	while(k--) {
		memset(&t_start, 0, sizeof(t_start));
		memset(&t_stop, 0, sizeof(t_stop));


		j=i;
		start_timing(t_start);
		while(j--) {
			 dummy.time = gettimeofday(&tp, NULL);
		}
		stop_timing(t_start);

		if(t_min_start.time > t_start.time) {
			t_min_start.time = t_start.time;
		}
	}

        printf("time_GTOD:\n\tGTOD: %llu (cycles)  %lf (micros)", t_min_start.time/i, (t_min_start.time/TSC)*1000000/i);

        return dummy.time;
}

unsigned long long time_slashproc() {
	volatile unsigned long long t_size[3], t_data[3], t_noop[3], t_min_size[3], t_min_data[3], t_min_noop[3];
        volatile int i = 1000, j = 0, k=100;
        double TSC = ktau_get_tsc();
	struct timeval tp;
	int pid = 0, index = 0;
	long dummy = 0;
	char* buffer = NULL;

	//set these to large values on startup
	for(index=0; index<3; index++) {
		t_min_size[index] = t_min_data[index] = t_min_noop[index] = ktau_rdtsc();
	}

	while(k--) {
		memset(t_size, 0, sizeof(t_size));
		memset(t_data, 0, sizeof(t_data));
		memset(t_noop, 0, sizeof(t_noop));


		j=i;
		while(j--) {
			dummy = read_size_timed(KTAU_TYPE_PROFILE, 1, &pid, 1, 0, NULL, -1.0, t_size);
		}
		dummy = dummy*10;
		buffer = (char*)calloc(dummy,1);
		if(!buffer) {
			printf("time_slashproc: calloc failed. Returning.\n");
			return -1;
		}

		j=i;
		while(j--) {
			dummy = read_data_timed(KTAU_TYPE_PROFILE, 1, &pid, 1, buffer, dummy,0, NULL, t_data);
		}
		free(buffer);
		buffer = NULL;

		j=i;
		while(j--) {
			dummy = ktau_noop_timed(t_noop);
		}

		for(index=0; index<3; index++) {
			if(t_min_size[index] > t_size[index]) {
				t_min_size[index] = t_size[index];
			}
			if(t_min_data[index] > t_data[index]) {
				t_min_data[index] = t_data[index];
			}
			if(t_min_noop[index] > t_noop[index]) {
				t_min_noop[index] = t_noop[index];
			}
		}
	}

        printf("time_slashproc:");
	for(index=0; index<3; index++) {
		printf("\nindex:%d\tPROC SIZE: %llu (cycles)  %lf (micros)", index, t_min_size[index]/i, (t_min_size[index]/TSC)*1000000/i);
		printf("\nindex:%d\tPROC DATA: %llu (cycles)  %lf (micros)", index, t_min_data[index]/i, (t_min_data[index]/TSC)*1000000/i);
		printf("\nindex:%d\tPROC NOOP: %llu (cycles)  %lf (micros)", index, t_min_noop[index]/i, (t_min_noop[index]/TSC)*1000000/i);
	}

        return dummy;
}


long double irq_catcher() {
	volatile long double limit = (2660.009 * 10000 * 0.5);
	volatile long double val1 = 53, val2 = 47;
	while(limit-- > 0.0001) {
		val1 += (val2*limit);
		val2 -= val1;
	}
	return val1;
}


void print_ktau_data(ktau_data* data) {
	printf("incl:%llu\texcl:%llu\tcount:%lu\n", data->timer.incl, data->timer.excl, data->timer.count);
}

void print_ctr_data(ktau_ush_ctr *ctr, ktau_data* data) {
	int i = 0;
	printf("CTR NAME: %s\n", ctr->desc.name);
	for(i = 0; i < ctr->desc.no_syms; i++) {
		printf("SYM: %lx\n", ctr->desc.syms[i]);
		print_ktau_data(&(data[i]));
	}
}

#define XSTR(sym) #sym
#define STR(sym) XSTR(sym)

timing_val timings[MAX_NO_REPS][MAX_OP];
timing_val timings_avg[MAX_OP];
timing_val timings_max[MAX_OP];
timing_val timings_min[MAX_OP];

int test_timing_shctr(int no_reps, int max_nr_syms) {
	ktau_ushcont* ushcont = NULL;
	ktau_ush_ctr *ush_ctr = NULL;
	char ctr_name[KTAU_MAX_COUNTERNAME] = "KTAU_TEST_CTR01";
	int nr_cont = 0, result = 1, ret = 0, nr_syms = 0, i = 0;
	unsigned long sym[32];
	ktau_data data[32];
	int pid = 0, rep = 0;
	unsigned long long excl[32];

	//setup sym to point to some instrumentation point - need to automate this
	sym[nr_syms++] = SYS_NANOSLEEP;
	sym[nr_syms++] = SCHEDULE_VOL;
	sym[nr_syms++] = ICMP_REPLY;
	sym[nr_syms++] = SYS_GETPID;
	sym[nr_syms++] = SCHEDULE; 
	sym[nr_syms++] = DO_PAGE_FAULT;
	sym[nr_syms++] = __DO_SOFTIRQ;
	sym[nr_syms++] = __DO_IRQ;
	sym[nr_syms++] = SMP_APIC_TIMER_INT;
	sym[nr_syms++] = TIMER_INTERRUPT;

	if(nr_syms < max_nr_syms) {
		printf("test_timing_shctr: max_nr_syms:%d is greater than available-nr_syms:%d. Setting max to avail.\n", max_nr_syms, nr_syms);
		max_nr_syms = nr_syms;
	}

	printf("FINAL CONFIG: No Reps: %d  Max Syms:%d\n", no_reps, max_nr_syms);

	for(rep = 0; rep< no_reps; rep++) {

		start_timing(timings[rep][OP_SHCONT_ADD]);
		//get a shcont
		ushcont = ktau_add_container(KTAU_TYPE_PROFILE, 1, NULL, 0, NULL, SHCONT_SIZE);
		stop_timing(timings[rep][OP_SHCONT_ADD]);
		if(!ushcont) {
			printf("\tktau_add_container: FAILED. size:%ld\n", SHCONT_SIZE);
		}

		start_timing(timings[rep][OP_SHCTR_ADD]);
		//get a counter in shcont
		ret = ktau_get_counter(ushcont, ctr_name, max_nr_syms, sym, &ush_ctr);
		stop_timing(timings[rep][OP_SHCTR_ADD]);
		if((ret) || (!ush_ctr)) {
			printf("\tktau_get_counter: FAILED. ret:%d\n", ret);
			exit(-1);
		}

		//copy it
		start_timing(timings[rep][OP_SHCTR_COPY]);
		ret = ktau_copy_counter(ush_ctr, data, max_nr_syms);
		stop_timing(timings[rep][OP_SHCTR_COPY]);
		if(ret) {
			printf("\tktau_copy_counter: FAILED. ret:%d\n", ret);
			exit(-1);
		}

		//excl copy it
		start_timing(timings[rep][OP_SHCTR_CPEXCL]);
		ret = ktau_copy_counter_excl(ush_ctr, excl, max_nr_syms);
		stop_timing(timings[rep][OP_SHCTR_CPEXCL]);
		if(ret) {
			printf("\tktau_copy_counter_excl: FAILED. ret:%d\n", ret);
			exit(-1);
		}
		
		
		//print it
		start_timing(timings[rep][OP_SHCTR_PRINT]);
		print_ctr_data(ush_ctr, data);
		stop_timing(timings[rep][OP_SHCTR_PRINT]);
		
		//put the counter back
		start_timing(timings[rep][OP_SHCTR_DEL]);
		ret = ktau_put_counter(ush_ctr);
		stop_timing(timings[rep][OP_SHCTR_DEL]);
		if(ret) {
			printf("\tktau_put_counter: FAILED. ret:%d\n", ret);
			exit(-1);
		}
		
		//put it back
		start_timing(timings[rep][OP_SHCONT_DEL]);
		ret = ktau_del_container(KTAU_TYPE_PROFILE, 1, NULL, 0, NULL, ushcont);
		stop_timing(timings[rep][OP_SHCONT_DEL]);
		if(ret) {
			printf("\tktau_del_container: FAILED. size:%ld ret:%d\n", SHCONT_SIZE, ret);
			exit(-1);
		}

	}//for rep

	return 0;
}

void print_timings(int no_reps) {
	int rep = 0, op = 0;
	double tsc = ktau_get_tsc();
	for(op = 0; op<MAX_OP; op++) {
		//init
		timings_min[op].time = ~((unsigned long long)(0));
		printf("OP:%d %s:\n", op, op_desc[op]);
		for(rep = 0; rep<no_reps; rep++) {
			//printf("%d]. %llu\n", rep, timings[rep][op].time);
			timings_avg[op].time += timings[rep][op].time;
			if(timings_max[op].time < timings[rep][op].time) {
				timings_max[op].time = timings[rep][op].time;
			} else if(timings_min[op].time > timings[rep][op].time) {
				timings_min[op].time = timings[rep][op].time;
			}
		}
		timings_avg[op].time /= no_reps;
		printf("\t OP AVG:%llu (cycles)  %lf (micros)\n", timings_avg[op].time, (timings_avg[op].time/tsc)*1000000);
		printf("\t OP MAX:%llu (cycles)  %lf (micros)\n", timings_max[op].time, (timings_max[op].time/tsc)*1000000);
		printf("\t OP MIN:%llu (cycles)  %lf (micros)\n", timings_min[op].time, (timings_min[op].time/tsc)*1000000);
	}

	time_the_timing();
	time_GTOD();
	time_slashproc();
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

	test_timing_shctr(no_reps, max_nr_syms);

	print_timings(no_reps);

	printf("\n=========*****==========\n");
}

