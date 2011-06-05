/*************************************************************
 * File         : user-src/src/tests/shcont.c
 * Version      : $Id: shcont.c,v 1.1.2.3 2007/08/08 19:57:20 anataraj Exp $
 *
 ************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ktau_proc_interface.h"

/* Test the ktau_add_container and ktau_del_container interfaces */
#define NR_CONTS 20
long shcont_sizes[NR_CONTS] = {
			4096,
			8192,
			16384,
			32768,
			65536,
			131072,
			262144,
			524288,
			1048576,
			2097152,
			4194304,
			8388608,
			16777216,
			33554432,
			67108864,
			134217728,
			268435456,
			536870912,
			1073741824,
			2147483648,
			};
/*
			4294967296
			};
*/

int test_adddel_shcont(int no_sizes) {
	ktau_ushcont* ushcont = NULL;
	int nr_cont = 0, result = 1, ret = 0;
	printf("TEST: test_adddel_shcont\n");
	printf("========================\n");
	for(nr_cont = 0; nr_cont < no_sizes; nr_cont++) {
		printf("\t SIZE: %d\n", shcont_sizes[nr_cont]);
		//get a shcont
		ushcont = ktau_add_container(KTAU_TYPE_PROFILE, 1, NULL, 0, NULL, shcont_sizes[nr_cont]);
		if(!ushcont) {
			printf("\tktau_add_container: FAILED. size:%ld\n", shcont_sizes[nr_cont]);
			result = 0;
			continue;
		}

		//put it back
		ret = ktau_del_container(KTAU_TYPE_PROFILE, 1, NULL, 0, NULL, ushcont);
		if(ret) {
			printf("\tktau_del_container: FAILED. size:%ld ret:%d\n", shcont_sizes[nr_cont], ret);
			result = 0;
			continue;
		}
	}

	printf("========================\n");
	return result;
}

#define DEFAULT_NOREPS 1

int main(int argc, char* argv[]) {
	int no_reps = DEFAULT_NOREPS, i = 0, no_sizes = NR_CONTS/2;
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
		printf("Bad no.sizes :%d, defaulting to:%d\n", no_sizes, NR_CONTS/2);
		no_sizes = NR_CONTS/2;
	}
	printf("\nNO_REPS:%d\tNO_INDEXES:%d\n=========*****==========\n", no_reps, no_sizes);
	for(i=0; i< no_reps; i++) {
		test_adddel_shcont(no_sizes);	
	}
	printf("\n=========*****==========\n");
}

