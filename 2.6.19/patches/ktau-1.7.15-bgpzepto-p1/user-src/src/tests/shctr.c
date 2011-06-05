/*************************************************************
 * File         : user-src/src/tests/shtr.c
 * Version      : $Id: shctr.c,v 1.1.2.3 2007/03/24 00:09:21 anataraj Exp $
 *
 ************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ktau_proc_interface.h"

#include "syms.h"

/* Test the ktau_get_counter, ktau_put_counter, ktau_copy_counter  interfaces */
#define SHCONT_SIZE 4096

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

int test_getcopyput_shctr() {
	ktau_ushcont* ushcont = NULL;
	ktau_ush_ctr *ush_ctr = NULL;
	char ctr_name[KTAU_MAX_COUNTERNAME] = "KTAU_TEST_CTR01";
	int nr_cont = 0, result = 1, ret = 0, nr_syms = 0, i = 0;
	unsigned long sym[32];
	ktau_data data[32];
	int pid = 0;

	printf("TEST: test_getcopyput_shctr\n");
	printf("========================\n");
	//get a shcont
	ushcont = ktau_add_container(KTAU_TYPE_PROFILE, 1, NULL, 0, NULL, SHCONT_SIZE);
	if(!ushcont) {
		printf("\tktau_add_container: FAILED. size:%ld\n", SHCONT_SIZE);
		result = 0;
	}

	//setup sym to point to some instrumentation point - need to automate this
	sym[nr_syms++] = SYS_NANOSLEEP;
	sym[nr_syms++] = SCHEDULE_VOL;
	/*sym[nr_syms++] = ICMP_REPLY;
	 sym[nr_syms++] = SYS_GETPID; */
	sym[nr_syms++] = SCHEDULE; 
	sym[nr_syms++] = DO_PAGE_FAULT;
	sym[nr_syms++] = __DO_SOFTIRQ;
	sym[nr_syms++] = __DO_IRQ;
	sym[nr_syms++] = SMP_CALL_FUNCTION_INT;
	sym[nr_syms++] = SMP_APIC_TIMER_INT;
	/*
	printf("NR_SYMS is:%d\n", nr_syms);
	//make sure its not pending
	sleep(1);
	pid = getpid();
	printf("pid is:%d\n", pid);
	irq_catcher();
	irq_catcher();
	*/

	//get a counter in shcont
	ret = ktau_get_counter(ushcont, ctr_name, nr_syms, sym, &ush_ctr);
	if((ret) || (!ush_ctr)) {
		printf("\tktau_get_counter: FAILED. ret:%d\n", ret);
		goto out_free_cont;
		result = 0;
	}

	//copy it
	ret = ktau_copy_counter(ush_ctr, data, nr_syms);
	if(ret) {
		printf("\tktau_copy_counter: FAILED. ret:%d\n", ret);
		result = 0;
	}
	
	//print it
	print_ctr_data(ush_ctr, data);
	
	//sleep a little more
	sleep(2);
	irq_catcher();
	irq_catcher();

	//copy it
	ret = ktau_copy_counter(ush_ctr, data, nr_syms);
	if(ret) {
		printf("\tktau_copy_counter: FAILED. ret:%d\n", ret);
		result = 0;
	}

	//print it
	print_ctr_data(ush_ctr, data);
	
	//sleep a little more
	sleep(2);
	irq_catcher();
	irq_catcher();

	pid = getpid();
	printf("pid is:%d\n", pid);

	ret = ktau_copy_counter(ush_ctr, data, nr_syms);
	if(ret) {
		printf("\tktau_copy_counter: FAILED. ret:%d\n", ret);
		result = 0;
	}
	//print it
	print_ctr_data(ush_ctr, data);
	
	//put the counter back
	ret = ktau_put_counter(ush_ctr);
	if(ret) {
		printf("\tktau_put_counter: FAILED. ret:%d\n", ret);
		result = 0;
	}
	
out_free_cont:
	//put it back
	ret = ktau_del_container(KTAU_TYPE_PROFILE, 1, NULL, 0, NULL, ushcont);
	if(ret) {
		printf("\tktau_del_container: FAILED. size:%ld ret:%d\n", SHCONT_SIZE, ret);
		result = 0;
	}

	printf("========================\n");
	return result;
}

#define DEFAULT_NOREPS 1

int main(int argc, char* argv[]) {
/*
	int no_reps = DEFAULT_NOREPS, i = 0, no_sizes = NR_CONTS;
	if(argc > 1) {
		no_reps = atoi(argv[1]);
	}
	if(no_reps <= 0) {
		printf("Bad no reps:%d, defaulting to:%d\n", no_reps, DEFAULT_NOREPS);
		no_reps = DEFAULT_NOREPS;
	}
	if(argc > 2) {
		no_sizes = atoi(argv[2]);
	}
	if(no_sizes <= 0 || no_sizes >= NR_CONTS) {
		printf("Bad no.sizes :%d, defaulting to:%d\n", no_sizes, NR_CONTS);
		no_sizes = NR_CONTS;
	}
	for(i=0; i< no_reps; i++) {
		test_adddel_shcont(no_sizes);	
	}
*/
	test_getcopyput_shctr();
	printf("\n=========*****==========\n");
}

