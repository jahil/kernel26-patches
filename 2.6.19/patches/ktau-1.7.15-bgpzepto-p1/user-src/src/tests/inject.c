/*************************************************************
 * File         : user-src/src/tests/inject.c
 * Version      : $Id: inject.c,v 1.1.2.1 2007/04/12 05:18:15 anataraj Exp $
 *
 ************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ktau_proc_interface.h"

unsigned long long nvals[9][KTAU_MAX_INJECT_VALS] = {
		{0,0,0,0,0,0,0,0},
		{100,100,100,100,100,100,100,100},
		{500,500,500,500,500,500,500,500},
		{2500,2500,2500,2500,2500,2500,2500,2500},
		{12500,12500,12500,12500,12500,12500,12500,12500},
		{62500,62500,62500,62500,62500,62500,62500,62500},
		{312500,312500,312500,312500,312500,312500,312500,312500},
		{1562500,1562500,1562500,1562500,1562500,1562500,1562500,1562500},
		{7812500,7812500,7812500,7812500,7812500,7812500,7812500,7812500}
};

volatile ktau_inject_dummy = 0;

//a piece of inline code that busy-loops for 'flag' number of times
static inline void ktau_inject_now(int lc_flag) {
	volatile unsigned long long a = 59, b = 43;
	while(lc_flag--) {
		a += (ktau_inject_dummy + (b/a) - 32);
	}
	ktau_inject_dummy = b+a;
}

unsigned long long ktau_rdtsc(void);
double ktau_get_tsc(void);

int time_noise_segment(int times) {
	int i = 1000, j = 0;
	unsigned long long start = 0, stop = 0, total = 0;
	j = i;
	while(j--) {
		start = ktau_rdtsc();
		ktau_inject_now(times);
		stop = ktau_rdtsc();
		total += (stop-start);
	}

	printf("Times:%d\ntotal: %llu, avg:%llu \n", times, total, total/i);
	printf("Micros: total: %lf, avg:%lf \n", (double)(total/ktau_get_tsc())*1000000, (double)((total/i)/ktau_get_tsc())*1000000);
	return 0;
}

int test_inject_noise(unsigned long long* pnvals) {

	int ret = ktau_inject_noise(pnvals, KTAU_MAX_INJECT_VALS);
	if(ret) {
		printf("\tktau_inject_noise: FAILED.");
		return -1;
	}

	return 0;
}

unsigned long long g_nvals[8] = {0, 0, 0, 0, 0, 0, 0, 0};

int main(int argc, char* argv[]) {
	int index = 1, i = 0;
	unsigned long long lcl_timer = 0;
	unsigned long long gbl_timer = 0;
	if(argc != 3) {
		printf("Usage: %s <inject into global-timer> <inject into local-timer>\n", argv[0]);
		return -1;
	}
	gbl_timer = strtoull(argv[1], NULL, 10);
	lcl_timer = strtoull(argv[2], NULL, 10);
	printf("Lcl Timer:%d, Gbl Timer:%d\n",lcl_timer, gbl_timer);
	
	g_nvals[0] = gbl_timer;
	g_nvals[1] = lcl_timer;

	test_inject_noise(g_nvals);
/*
	time_noise_segment(5000);
	time_noise_segment(25000);
	time_noise_segment(50000);
	time_noise_segment(100000);
	time_noise_segment(130000);
	time_noise_segment(200000);
	time_noise_segment(300000);
	time_noise_segment(312500);
*/
	printf("\n=========*****==========\n");
}

