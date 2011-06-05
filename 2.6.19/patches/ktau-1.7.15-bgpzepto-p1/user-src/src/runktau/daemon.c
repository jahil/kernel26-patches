#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include <asm/unistd.h>

//#define __NR_sched_setaffinity  241
//_syscall3(int, sched_setaffinity, int, pid, int, msize, unsigned long*, mptr);

int main(int argc, char* argv[]) {
	volatile double a = 0, b = 37, c = 53, d = 11, i = 0, res = 0, k = 0;
	struct timeval tv1, tv2;
	int canrun = 1, dur = 0;
	unsigned long mask = 1;
	int no_cpus = 0;

	if(argc < 4) {
		printf("%s <tot no cpus> <processor no> <secs to run>", argv[0]);
		exit(-1);
	}

	no_cpus = atoi(argv[1]);

	mask = mask << ((atoi(argv[2])) % no_cpus);

	printf("mask:%x\tsched_affinity ret:%d\n",mask, sched_setaffinity(getpid(), sizeof(mask), &mask));

	dur = atoi(argv[3]);

        gettimeofday(&tv1, NULL);

	while(canrun) {

		while(k<1000000) {
		//while(1) {
			b = a*d/c+(30 - i++);
			a = c/(d+i);
			res = (b- (a/c));
			if(i>1009) i = 1009 - i - res;
			if(i < 0) i = 48;
			k++;
		}
		k = 0;	
		//sleep(dur/2);
	        gettimeofday(&tv2, NULL);
		if((tv2.tv_sec - tv1.tv_sec) >= dur) {
			canrun = 0;
		}
	}

	return 0;
}


