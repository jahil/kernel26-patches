
#include <sys/time.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/utsname.h>


#if ( defined(KTAUD_PPC) || defined(KTAUD_PPC64) )
/*****************************************
 * Modified by  : Suravee Suthikulpanit <suravee@mcs.anl.gov>
 * ARCH         : PowerPC
 * Description  : 
 *      The low-level PPC-class timer    
 * Reference: 
 *      Programing Environment Manual for 32-Bit Microprocessors 
 *      (Motorola), MPCFPE32B.pdf P.2-16 
 * Description:
 *      This code accesses the 64-bit VEA-Time Base (TB) register set
 *      of PowerPC processor. The register set is devided into upper (TBU)
 *      and lower (TBL) 32-bit register.        
 * loop: 
 *      mftbu   rx      #load from TBU 
 *      mftb    ry      #load from TBL 
 *      mftbu   rz      #load from TBU 
 *      cmpw    rz,rx   #see if old = new 
 *      bne     loop    #loop if carry occurred
*****************************************/
static __inline__ unsigned long long int rdtsc(void)
{
        unsigned long long int result=0;
        unsigned long int upper, lower,tmp;
        __asm__ __volatile__(
                "loop:                  \n"
                "\tmftbu   %0           \n"
                "\tmftb    %1           \n"
                "\tmftbu   %2           \n"
                "\tcmpw    %2,%0        \n"
                "\tbne     loop         \n"
                /*outputs*/: "=r"(upper),"=r"(lower),"=r"(tmp)
        );
        result = upper;
        result = result<<32;
        result = result|lower;
        /*printf("u = %20x\nl = %20x\nresult = %20llx\n",upper,lower,result);*/
        return(result);
}
#endif /*define(KTAUD_PPC) || define(KTAUD_PPC64) */

#ifdef KTAUD_PENTIUM
/*****************************************/
/* The low-level Pentium-class timer     */
/*****************************************/
static __inline__ unsigned long long int rdtsc()
{
     unsigned long long int x;
     __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
     return x;
}
#endif /* KTAUD_PENTIUM */

#ifdef KTAUD_x86_64
/*****************************************/
/* The low-level x86_64-class timer     */
/*****************************************/
static __inline__ unsigned long long int rdtsc()
{
     unsigned int x = 0, y = 0;
     __asm__ volatile ("rdtsc" : "=a" (x), "=d" (y));
     return ( ((unsigned long)x) | (((unsigned long)y)<<32) );
}
#endif /* KTAUD_x86_64 */

#ifdef KTAUD_MIPS64
/*****************************************/
/* MIPS sigh */
/*****************************************/
#include <time.h>
static __inline__ unsigned long long int rdtsc()
{
#if 0
     unsigned long long int x;
     struct timespec t;
     clock_gettime(CLOCK_SGI_CYCLE, &t);
     x = ( (t.tv_sec * 1000000) + (t.tv_nsec / 1000)  );
     return x;
#endif //0
#warning rdtsc not available from user-space
	return 0;
}
#endif /* KTAUD_MIPS64 */

unsigned long long ktau_rdtsc() {
	return rdtsc();
}

/*****************************************/
/* Simple clock to return seconds        */
/*****************************************/
static double quicksecond()
{
  struct timeval tp;
  struct timezone tzp;
  int i;

  i = gettimeofday(&tp,&tzp);
  return ( (double) tp.tv_sec + (double) tp.tv_usec * 1.e-6 );
}

/*****************************************/
/* Simple clock to return microseconds        */
/*****************************************/
static double quickmicrosecond()
{
  struct timeval tp;
  struct timezone tzp;
  int i;

  i = gettimeofday(&tp,&tzp);
  return ( (double) tp.tv_sec * 1.e6 + (double) tp.tv_usec );
}

/*****************************************/
/* Calc quick (effective) cycles/sec appx for runtimes */

/* Since each CPU can be a different speed, it is convenient to run
   the benchmark based on some total wallclock, rather than the number
   of ticks.  This function simply does a quick ballpark calibration
   to find the number of ticks per second, so the benchmarks can be
   run some fixed number of ticks, and the completion time can be
   conveniently estimated */
/*****************************************/
double cycles_per_sec(void) {
  double start, elapsed, accum=0.0, y;
  int i, flipper=1;
  unsigned long long int x;

  /*if (verbose) printf("Calibrating benchmark loop size...     \n"); */

  /*print_run_info();*/   /* Print information about this benchmark */

  x = rdtsc();
  start=quicksecond();

  /* repeat until at least 5 secs have elapsed */
  while ( (elapsed=quicksecond()-start) < 1) {

    if (flipper == 1) flipper=-1; else flipper=1;

    for (i=0; i<1000000; i++) {
      /* this is a complicated computation to avoid being removed by
         removed by the optimizer, and floating point overflow */
      accum = accum + (i * (double) flipper);
    }
  }

  x = rdtsc() - x;  /* cycles elapsed */
  elapsed = quicksecond() - start;   /* time elapsed */

  y = (double) x / elapsed; /* cycles per second (approx.) */

  return y;
}


double ktau_get_tsc(void)
{
        FILE *f = NULL;
        double tsc= 0;
        char *cmd = NULL;
        char buf[BUFSIZ];
        struct utsname machine_info;

        /* Read uname -m */
        uname(&machine_info);

        /* Command */
        if(!strcmp(machine_info.machine, "ppc")){
                cmd = "cat /proc/cpuinfo | egrep -i '^timebase' | head -1 | sed 's/^.*: //'";
        }else if(!strcmp(machine_info.machine, "i686")){
                cmd = "cat /proc/cpuinfo | egrep -i '^cpu MHz' | head -1 | sed 's/^.*: //'";
         }else if(!strcmp(machine_info.machine, "x86_64")){
                cmd = "cat /proc/cpuinfo | egrep -i '^cpu MHz' | head -1 | sed 's/^.*: //'";
         }else if(!strcmp(machine_info.machine, "mips64")){
                cmd = "cat /proc/cpuinfo | egrep -i '^BogoMIPS' | head -1 | sed 's/^.*: //'";
	}

        if ((f = popen(cmd,"r"))!=NULL){
                while (fgets(buf, BUFSIZ, f) != NULL) {
                        tsc = atof(buf);
                        if(!strcmp(machine_info.machine, "ppc")){
                                /* For PPC, timebase counter */
                                printf("TSC: %f\n", tsc);
                        }else if(!strcmp(machine_info.machine, "i686")){
                                /* For i686, timestamp counter in double MHz */
                                tsc = tsc * 1e6;
                                printf("TSC: %f\n", tsc);
                        }else if(!strcmp(machine_info.machine, "x86_64")){
                                /* For x86_64,timestamp counter in double MHz */
                                tsc = tsc * 1e6;
                                printf("TSC: %f\n", tsc);
                        }else if(!strcmp(machine_info.machine, "mips64")){
                                /* timestamp counter in double MHz */
                                tsc = tsc * 1e6;
				//for sico - the cycle counter counts once per two cycles
                                printf("TSC: %f Ctr: %f\n", tsc, tsc/2);
				tsc = tsc/2;
                        }
                }
        }
        
        /* If there is no timestamp counterinformation
         * in the /proc/cpuinfo, we have to run a test
         */
        if(tsc == 0){ 
                tsc = cycles_per_sec();
                printf("TSC: %f\n",tsc);
        }
        pclose(f);
        return tsc;
}

