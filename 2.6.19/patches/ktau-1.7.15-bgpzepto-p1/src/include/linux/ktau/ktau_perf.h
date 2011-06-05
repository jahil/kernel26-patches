/*****************************************************
 * File    :    include/linux/ktau/ktau_perf.h
 * Version :    "$Id: ktau_perf.h,v 1.2 2006/11/12 00:26:28 anataraj Exp $;
 ****************************************************/

#ifndef _KTAU_PERF_H_
#define _KTAU_PERF_H_

//#ifdef CONFIG_KTAU_PERF_COUNTER
unsigned int dummy_ktau_perf_func(void){
	return 0;
}

typedef unsigned int (*_ktau_perf_func)(void);

_ktau_perf_func ktau_perf_func[] = 
	{
		&dummy_ktau_perf_func,		/* 0 */
		NULL,		/* 1 */
		NULL,		/* 2 */
		NULL,		/* 3 */
		NULL,		/* 4 */
		NULL,		/* 5 */
		NULL,		/* 6 */
		NULL,		/* 7 */
		NULL,		/* 8 */
		NULL,		/* 9 */
		NULL,		/* 10 */
		NULL,		/* 11 */
		NULL,		/* 12 */
		NULL,		/* 13 */
		NULL,		/* 14 */
		NULL		/* 15 */
	};

//#endif /*CONFIG_KTAU_PERF_COUNTER*/
#endif /* _KTAU_PERF_H_ */
