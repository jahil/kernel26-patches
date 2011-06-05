/***********************************************
 * File         : include/linux/ktau/ktau_diff_profiles.h
 * Version      : $Id: ktau_diff_profiles.h,v 1.1 2006/11/12 00:26:29 anataraj Exp $
 ***********************************************/

#ifndef _KTAU_DIFF_PROFILES_H
#define _KTAU_DIFF_PROFILES_H

#include <linux/ktau/ktau_datatype.h>
#include <ktau_proc_interface.h>

#ifdef __cplusplus
extern "C" {
#endif

int ktau_diff_profiles(ktau_output* plist1, int size1, ktau_output* plist2, int size2, ktau_output** outlist);

#ifdef __cplusplus
} //extern "C"
#endif /* __cplusplus */

#endif /* _KTAU_DIFF_PROFILES_H */
