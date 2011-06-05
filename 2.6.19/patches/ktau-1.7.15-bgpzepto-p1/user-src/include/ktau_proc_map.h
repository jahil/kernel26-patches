/***********************************************
 * File         : include/linux/ktau/ktau_proc_map.h
 * Version      : $Id: ktau_proc_map.h,v 1.1 2006/11/12 00:26:29 anataraj Exp $
 ***********************************************/

#ifndef _KTAU_PROC_MAP_H
#define _KTAU_PROC_MAP_H

#ifdef __cplusplus
extern "C" {
#endif


//fwd declaration
struct ktau_sym_map;

/* ktau_lookup_sym : Given an addr, returns Symbol *
 ***************************************************
 * addr: the addr to lookup
 *
 * pmap: pointer to the map (obtained from ktau_get_kallsyms)
 *
 * Returns:
 * symbol name as Null terminated C string.
 */
extern const char* ktau_lookup_sym(unsigned long long addr, struct ktau_sym_map* pmap);


/* ktau_put_kallsyms: Reverse of the ktau_get_kallsyms
 * This is needed to prevent resource leaks (memory).
 * When a ktau_sym_map* is no longer need
 * it must be returned.
 ***************************************************
 * pmap: ktau_sym_map* that was obtained from 
 * ktau_get_kallsyms.
 *
 * Returns:
 * void
 *
 * Note:
 * Once this is called, the pmap can 
 * no longer be used.
 */
extern void ktau_put_kallsyms(struct ktau_sym_map* pmap);


/* ktau_get_kallsyms:
 * Given a mapping file path,
 * provides an "opaque" ktau_sym_map*.
 *
 * ktau_lookup_sym can be used on it.
 *
 * Once not needed, it must be returned using
 * ktau_put_kallsyms.
 ***************************************************
 * filepath: file path of location of kernel
 * symbol map file (System.map)
 * Returns:
 * "opaque" ktau_sym_map ptr
 * NULL on error.
 */
extern struct ktau_sym_map* ktau_get_kallsyms(char* filepath);


#ifdef __cplusplus
} //extern "C"
#endif /* __cplusplus */

#endif /* _KTAU_PROC_MAP_H */
