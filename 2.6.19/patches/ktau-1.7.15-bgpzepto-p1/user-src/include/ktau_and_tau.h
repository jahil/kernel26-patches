
#ifndef _KTAU_AND_TAU_H_
#define _KTAU_AND_TAU_H_

#include <ktau_proc_interface.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cpluscplus */

extern int print_tau_profiles(ktau_output* inprofiles, int no_profiles, int nodeid, struct ktau_sym_map* psymmap, unsigned long long timer_res_sec);

#ifdef __cplusplus
} //extern "C"
#endif /* __cpluscplus */

#endif /* _KTAU_AND_TAU_H_ */
