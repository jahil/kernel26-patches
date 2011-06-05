
#include <sys/time.h>

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
__inline__ unsigned long long int rdtsc(void)
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
__inline__ unsigned long long int rdtsc()
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

/*****************************************/
/* Simple clock to return seconds        */
/*****************************************/
double quicksecond();
#if 0
double quicksecond()
{
  struct timeval tp;
  struct timezone tzp;
  int i;

  i = gettimeofday(&tp,&tzp);
  return ( (double) tp.tv_sec + (double) tp.tv_usec * 1.e-6 );
}
#endif /*0*/
/*****************************************/
/* Simple clock to return microseconds        */
/*****************************************/
double quickmicrosecond();
#if 0
double quickmicrosecond()
{
  struct timeval tp;
  struct timezone tzp;
  int i;

  i = gettimeofday(&tp,&tzp);
  return ( (double) tp.tv_sec * 1.e6 + (double) tp.tv_usec );
}
#endif /*0*/
/*****************************************/
/* Calc quick (effective) cycles/sec appx for runtimes */

/* Since each CPU can be a different speed, it is convenient to run
   the benchmark based on some total wallclock, rather than the number
   of ticks.  This function simply does a quick ballpark calibration
   to find the number of ticks per second, so the benchmarks can be
   run some fixed number of ticks, and the completion time can be
   conveniently estimated */
/*****************************************/
double cycles_per_sec(void);
#if 0
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
#endif /*0*/
