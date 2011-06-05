/******************************************************************************
 * Version	: $Id: ktau_cont.c,v 1.1.2.16 2008/11/19 05:26:50 anataraj Exp $
 * ***************************************************************************/ 

//std kernel headers
#include <linux/kernel.h>
#include <linux/sched.h>

//debug support
//#define TAU_DEBUG
#define TAU_NAME "ktau_cont"
#include <linux/ktau/ktau_print.h>

#include <linux/ktau/ktau_hash.h>
#include <linux/ktau/ktau_api.h>
#include <linux/ktau/ktau_cont_data.h>
#include <linux/ktau/ktau_cont.h>
#include <linux/ktau/ktau_mmap.h>

#ifdef CONFIG_ZEPTO_MEMORY
#include <linux/compclass.h>                    /* Zepto Compute Class checking */
#endif

/* ktau_init_kshcont:
 * Intialize a kernel-shcont after mapping-in
 * the user-buf.
 * 
 * [inout] k_shcont:	ptr to ktau_shcont 
 * [in] ushcont:	ptr to ktau_ushcont 
 * [in] orig_kaddr:	from ktau_map_userbuf
 * [in] kaddr:		from ktau_map_userbuf
 * [in] pages:		from ktau_map_userbuf
 * [in] nr_pages:	from ktau_map_userbuf
 * 
 * Return	:	void.
 *---------------------------------------------------*/
void ktau_init_kshcont(ktau_shcont* k_shcont, ktau_ushcont* ushcont, unsigned long orig_kaddr, unsigned long kaddr, struct page** pages, int nr_pages) {

	//KID
	k_shcont->kid = 0; //currently 0 is possible
	
	//uaddr, kaddr, orig_kaddr
	k_shcont->uaddr = (unsigned long)ushcont->buf;
	k_shcont->kaddr = kaddr;
	k_shcont->orig_kaddr = orig_kaddr;

	//pages, nr_pages
	k_shcont->pages = pages;
	k_shcont->nr_pages = nr_pages;

	//others
	k_shcont->size = ushcont->size;
	k_shcont->next_off = 0; //to begin
	k_shcont->flags = ushcont->flags;
	k_shcont->ref_cnt = 1; //to begin

	//buffer
	k_shcont->buf = (ktau_dbl_buf*)kaddr;
	//assume the dbl_buf is being inited by uspace?
	//atleast set cur_index to 0;
	k_shcont->buf->cur_index = 0;
}


/* ktau_deinit_kshcont:
 * De-Intialize a kernel-shcont before un-mapping
 * the user-buf.
 * 
 * [in] task	:	struct task_struct ptr (sually current)
 * [inout] k_shcont:	ptr to ktau_shcont 
 * 
 * Return	:	void.
 *---------------------------------------------------*/
void ktau_deinit_kshcont(struct task_struct* task, ktau_shcont* shcont) {
	shcont->ref_cnt--;
	//just do a memset?
	shcont->on = 0;
	shcont->uaddr = shcont->kaddr = shcont->orig_kaddr = 0;
	shcont->pages = shcont->buf = NULL;
	shcont->nr_pages = shcont->size = shcont->next_off = 0;

	task->ktau->no_shconts--;
}

/* ktau_test_kshcont:
 * Tests to see if a *newly inited* kshcont
 * actually points to correctly mapped-in
 * user-space memory. The user-land should 
 * have placed a particular byte sequence...
 *
 * [in] k_shcont:	ptr to ktau_shcont 
 * 
 * Return	:	1 on Pass, 0 on failure.
 *
 * NOTE: This should be called only right after initing.
 * Otherwise may fail.
 *---------------------------------------------------*/
int ktau_test_kshcont(ktau_shcont* k_shcont) {
	int *val1, *val2;
	//val1 = (int*)((unsigned long)k_shcont->buf + sizeof(ktau_dbl_buf));
	//val2 = (int*)((unsigned long)val1 + k_shcont->size);
	val1 = (int*)(ktau_shcont2dblbuf(k_shcont, 0));
	val2 = (int*)(ktau_shcont2dblbuf(k_shcont, 1));
	
	info("ktau_test_kshcont: *val1:%x  *val2:%x  (*val1 & *val2):%x  (0xdeadbeef & 0xbeefdead):%x\n", *val1, *val2, (*val1 & *val2), (0xdeadbeef & 0xbeefdead));

	if((*val1 & *val2) != (0xdeadbeef & 0xbeefdead)) {
		return 0;
	}

	//also setup a test for checking kspace-->uspace data sharing direction
	*val1 = 0xbadbeef;
	*val2 = 0xbeefbad;

	return 1;
}


/* ktau_share_cont:
 * Maps the user-space shcont into
 * kernel address-space using ktau_mmap_userbuf.
 * Then it initializes the ktau_shcont and registers
 * it with the task_struct (current).
 * Increments reference count.
 * 
 * [in] task	:	task_struct (usually current)
 * [in] ushcont:	ptr to ktau_ushcont 
 *			(a copy of the one from uspace)
 * 
 * Return	:	returns ptr to a new ktau_shcont 
 *			or NULL on error.
 *---------------------------------------------------*/
ktau_shcont* ktau_share_cont(struct task_struct* task, ktau_ushcont* ushcont) {
	int retval = 0, nr_pages = 0;
	unsigned long orig_kaddr = 0, kaddr = 0;
	struct page **pages = NULL;
	ktau_shcont* k_shcont = NULL;

	info("ktau_share_cont: Enter: pid:%d \n",  task->pid);

	if(task->ktau->no_shconts >= KTAU_MAX_SHCONTS) {
		err("ktau_share_cont: no_shconts[%d] >= KTAU_MAX_SHCONTS[%d].\n", task->ktau->no_shconts, KTAU_MAX_SHCONTS);
		return NULL;
	}

	//try validating user-mem
	if(ktau_validate_umem((void*)ushcont->buf, ktau_dblbuf_size(ushcont->size))) {
		err("ktau_share_cont: Bad user-mem passed in? ktau_validate_umem failed. Aborting.");
		return NULL;
	}

#ifdef CONFIG_ZEPTO_MEMORY
	if(IS_COMPCLASS(current)) {
		orig_kaddr = kaddr = (unsigned long)ushcont->buf;
		pages = NULL;
		nr_pages = 0;
	} else
#endif /* CONFIG_ZEPTO_MEMORY */
	{
		retval = ktau_map_userbuf(task, (unsigned long)ushcont->buf, ktau_dblbuf_size(ushcont->size), &pages, &nr_pages, &orig_kaddr, &kaddr);
		if((retval < 0) || (kaddr == 0)) {
			err("ktau_share_cont: ktau_map_userbuf ret err.\n");
			return NULL;
		}		
	} 
	
	k_shcont = &(task->ktau->shconts[0]);
	task->ktau->no_shconts++;

	//Initialize the kernel shcont struct
	k_shcont->zcb_on = 0;
	ktau_init_kshcont(k_shcont, ushcont, orig_kaddr, kaddr, pages, nr_pages);
#ifdef CONFIG_ZEPTO_MEMORY
	if(IS_COMPCLASS(current)) {
		k_shcont->zcb_on = 1;
	}
#endif /* CONFIG_ZEPTO_MEMORY */

	//Run a small tet to see if this is indeed shared-mem
	if(!ktau_test_kshcont(k_shcont)) {
		//failed the test
		err("ktau_share_cont: ktau_test_kshcont: failed test\n.");
		goto out_unmap;
	}

	info("ktau_share_cont: ktau_test_kshcont: Passed K-test.\n");

	//enable it
	k_shcont->on = 1;
	
	info("ktau_share_cont: Exit: pid:%d \n",  task->pid);

	return k_shcont;
	
out_unmap:
	if(!k_shcont->zcb_on) {
		ktau_unmap_userbuf(task->pid, pages, nr_pages, orig_kaddr);
	}
	ktau_deinit_kshcont(task, k_shcont);

	err("ktau_share_cont: ERROR: Exit: pid:%d \n",  task->pid);

	return NULL;
}



/* ktau_unshare_cont:
 * Unmaps the user-space shcont from
 * kernel address-space using ktau_unmap_userbuf.
 * Before that it unregisters the shcont from 
 * task_ctruct (usually current).
 * 
 * [in] task	:	task_struct (usually current)
 * [in] ushcont	:	ptr to ktau_ushcont 
 *			(a copy of the one from uspace)
 * 
 * Return	:	(int) 0 on Success. -ve on error.
 *
 * Constraints	:	Reference count of shcont MUST
 *			be 1 when this is called. 
 *			If ref_cnt > 1, then some object
 *			(e.g. counter) is still residing
 *			in the container.
 *			Then, return (ref_cnt). 0 on
 *			Success, else other -ve error.
 *
 *---------------------------------------------------*/
int ktau_unshare_cont(struct task_struct* task, ktau_ushcont* ushcont) {

	ktau_shcont* shcont = NULL;

	info("ktau_unshare_cont: enter: task->pid:%d  ushcont:%p\n", task->pid, ushcont);

	if(task->ktau->no_shconts <= 0) {
		err("ktau_unshare_cont: no_shconts <= 0: %d\n", task->ktau->no_shconts);
		return -1;
	}
	
	//using the ushcont->kid, get the k_shcont
	info("ktau_unshare_cont: ushcont->kid:%d\n", ushcont->kid);
	if((ushcont->kid < 0) || (ushcont->kid >= KTAU_MAX_SHCONTS)) {
		err("ktau_unshare_cont: bad kid:%d\n", ushcont->kid);
		return -1;
	}
	shcont = &(task->ktau->shconts[ushcont->kid]);

	//got the kernel-shcont, now try to validate and unshare
	if(shcont->ref_cnt != 1) {
		err("ktau_unshare_cont: shconts->ref_cnt != 1: %d\n", shcont->ref_cnt);
		return shcont->ref_cnt;
	}

	if(!shcont->zcb_on) {
		ktau_unmap_userbuf(task->pid, shcont->pages, shcont->nr_pages, shcont->orig_kaddr);
	}

	ktau_deinit_kshcont(task, shcont);

	info("ktau_unshare_cont: exit.");

	return 0;
}


static ktau_shcont* ktau_lookup_kshcont(struct task_struct* tsk, int shcont_kid) {
	//indexing or a list (or a hash)?
	//start with index and change later
	if(shcont_kid >= tsk->ktau->no_shconts) {
		err("ktau_lookup_kshcont: kid[%d] Exceeds max avail no_shconts[%d].", shcont_kid, tsk->ktau->no_shconts);
		return NULL;
	}

	return &(tsk->ktau->shconts[shcont_kid]);
}

int register_ctr_shcont(ktau_shcont* k_shcont, ktau_sh_ctr* k_shctr) {
	unsigned long ctr_size = 0;
	ctr_size = ktau_shctr_data_size(k_shctr);
	//check if size avail
	if(ctr_size > (k_shcont->size - k_shcont->next_off)) {
		err("register_ctr_shcont: ctr_size[%lu] > avail shcont size[%lu].", ctr_size, (k_shcont->size - k_shcont->next_off));
		return -1;
	}

	k_shctr->offset = k_shcont->next_off;
	k_shcont->next_off += ctr_size;
	k_shcont->ref_cnt++;
	k_shctr->cont = k_shcont;

	return 0;
}

int unregister_ctr_shcont(ktau_sh_ctr* k_shctr) {
	k_shctr->cont->ref_cnt--;
	k_shctr->cont = NULL;
	//we dont currently "free" the space in the container
	//as of now the container is not general-purpose
	//instead, it is used to add a bunch of counters at
	//one go - the counters are assumed to be removed as
	//a whole. Although its not required - the space in
	//container is not reusable. Simple to fix - but now now.
	return 0;
}


/* create a ktau_sh_ctr, init it and register with container */
static ktau_sh_ctr* init_shctr(struct task_struct *tsk, ktau_ush_ctr* u_shctr, int shcont_kid) {
	ktau_sh_ctr* k_shctr = NULL;
	ktau_shcont* k_shcont = NULL;
	int ret = 0;
	unsigned long kflags = 0;

	//find shcont_kid cont
	k_shcont = ktau_lookup_kshcont(tsk, shcont_kid);
	if(!k_shcont) {
		err("init_shctr: ktau_lookup_kshcont on kid:%d failed.", shcont_kid);
		return NULL;
	}

	//alloc k_shctr
	k_shctr = (ktau_sh_ctr*)kmalloc(sizeof(ktau_sh_ctr), GFP_KERNEL);
	if(!k_shctr) {
		err("init_shctr: kmalloc of ktau_sh_ctr failed.");
		return NULL;
	}
	//init k_shctr 
	k_shctr->type = u_shctr->type;
	k_shctr->desc = u_shctr->desc;
	k_shctr->triggers = NULL;

	//acquire lock
	ktau_spin_lock(&tsk->ktau_lock,kflags);

	ret = register_ctr_shcont(k_shcont, k_shctr);
	if(ret) {
		err("init_shctr: register_ctr_shcont returned error [%d].", ret);
		goto out_free;
	} 
	//assign an id (TODO: asign a better id)
	k_shctr->desc.kid = (unsigned long)k_shctr;
	//increment number
	tsk->ktau->no_shctrs++;
	//put on list
	//add this to list pointed to by task_struct->ktau->shctr
	list_add(&(k_shctr->list), &(tsk->ktau->shctr_list));

	//release lock
	ktau_spin_unlock(&tsk->ktau_lock,kflags);

	info("init_shctr: Exit.");
	return k_shctr;
out_free:
	kfree(k_shctr);
	//release lock
	ktau_spin_unlock(&tsk->ktau_lock,kflags);
	return NULL;
}

/* remove a ktau_sh_ctr, unregister with container , unhook from Qs *
 * and free it. */
static void deinit_shctr(struct task_struct *tsk, ktau_sh_ctr* k_shctr) {
	unsigned long kflags = 0;

	info("deinit_shctr: Enter: pid:%d k_shctr:%p.", tsk->pid, k_shctr);

	//acquire lock
	ktau_spin_lock(&tsk->ktau_lock,kflags);

	//del from list
	list_del(&(k_shctr->list));
	tsk->ktau->no_shctrs--;

	//unreg from container
	if(unregister_ctr_shcont(k_shctr)) {
		err("deinit_shctr: unregister_ctr_shcont Failed.... Wierd. Proceeding...");
	}
	//free mem
	kfree(k_shctr);

	//release lock
	ktau_spin_unlock(&tsk->ktau_lock,kflags);

	info("deinit_shctr: Exit.");
	return;
}

/* ktau_ctr_create:
 * Creates a shared counter, associates it with
 * the requested sh-container, registers all the
 * events that the counter expresses interest in.
 * Increments reference count of shared-container.
 * Sets the k_id of the counter (so that it can be
 * located later by calls from uspace calls).
 * 
 * [in] task	:	task_struct (usually current)
 * [inout] u_shctr:	ptr to ktau_ush_ctr  
 *			(a copy of the one from uspace
 *			 recieved thro /proc).
 * [in]	kid	:	the id of the req. sh-container
 * Return	:	0 on SUCESS.
 *			-ve on error. (error codes TODO).
 *---------------------------------------------------*/
int ktau_ctr_create(struct task_struct* task, ktau_ush_ctr* u_shctr, int shcont_kid) {
	int ret = 0;
	ktau_sh_ctr* k_shctr = NULL;
	
	info("ktau_ctr_create: Enter: task->pid:%d u_shctr:%p shcont_kid:%d", task->pid, u_shctr, shcont_kid);

	/* create a ktau_sh_ctr, init it and register with container */
	k_shctr = init_shctr(task, u_shctr, shcont_kid);
	if(!k_shctr) {
		err("ktau_ctr_create: init_shctr ret NULL.");
		return -1;
	}

	/* find and register with events that ctr interested in	*
	 * , use the default shared-ctr handler - for now. 	*/
	ret = ktau_reg_ctrevents(task, k_shctr, ktau_shctr_handler_def);
	if(ret < 0) {
		/* either -ve error, or 0 events could be registered! */
		err("ktau_ctr_create: register_ctr_events ret < 0[%d].", ret);
		goto out_free;
	}

	//write back into u_shctr those fields that need to be set
	u_shctr->offset = k_shctr->offset;
	u_shctr->desc.kid = k_shctr->desc.kid;
	info("ktau_ctr_create: u_shctr new kid:%lx.", u_shctr->desc.kid);
	//dont know u_shctr->ucont, that needs to be set in uspace

	info("ktau_ctr_create: Exit.");
	return 0;

out_free:
	deinit_shctr(task, k_shctr);
	return -1;
}


static ktau_sh_ctr* ktau_lookup_kshctr(struct task_struct* tsk, unsigned long kid) {
	ktau_sh_ctr *tmp;
	struct list_head *pos;
	if(!tsk->ktau->no_shctrs) {
		info("ktau_lookup_kshctr: no_shctrs==0; Not Found kid:%lx (the head).", kid);
		return NULL;
	}
	list_for_each(pos, &(tsk->ktau->shctr_list)) {
		tmp = list_entry(pos, ktau_sh_ctr, list);
		info("ktau_lookup_kshctr: checking shctr entry kid:%lx.", tmp->desc.kid);
		if(tmp->desc.kid == kid) {
			info("ktau_lookup_kshctr: Found kid:%lx.", kid);
			return tmp;
		}
	}//list_for_each
	
	info("ktau_lookup_kshctr: NOT Found kid:%lx.", kid);

	return NULL;
}


/* ktau_ctr_delete:
 * finds the kernel-counter from the k_id, 
 * deletes a shared counter, dissociates it from
 * its sh-container, unregisters all the
 * events that the counter expressed interest in.
 * Decrements reference count of shared-container.
 * 
 * [in] task	:	task_struct (usually current)
 * [inout] u_shctr:	ptr to ktau_ush_ctr  
 *			(a copy of the one from uspace
 *			 recieved thro /proc).
 * Return	:	0 on SUCESS.
 *			-ve on error. (error codes TODO).
 *---------------------------------------------------*/
int ktau_ctr_delete(struct task_struct* task, ktau_ush_ctr* u_shctr) {
	int ret;
	ktau_sh_ctr* k_shctr = NULL;
	
	info("ktau_ctr_delete: Enter: task->pid:%d u_shctr:%p u_shctr->desc.kid:%lx .", task->pid, u_shctr, u_shctr->desc.kid);

	k_shctr = ktau_lookup_kshctr(task, u_shctr->desc.kid);
	if(!k_shctr) {
		err("ktau_ctr_delete: ktau_lookup_kshctr cannot find kid:%lx.", u_shctr->desc.kid);
		return -1;
	}

	/* find and un-register events */
	ret = ktau_unreg_ctrevents(task, k_shctr);
	if(ret < 0) {
		err("ktau_ctr_delete: unregister_ctr_events ret < 0[%d].", ret);
		return -1;
	}

	/* dealloc mem etc, untie from container, decrease cont ref_cnt */
	deinit_shctr(task, k_shctr);
	
	info("ktau_ctr_delete: Exit.");
	return 0;
}


/* ktau_shctr_handler_def:
 * Default shared-counter event handler.
 * Returns 0 on success. -ve on error.
 * Usually called from startr/stop inst. routines
 */
int ktau_shctr_handler_def(ktau_trigger* ev, int type, unsigned long long incl, unsigned long long excl) {
	ktau_data* ctr_data = NULL;
	//point to dbl_buf
	ktau_dbl_buf* buf = NULL;
	//void *__ptr1, *__ptr2, *__ptr3;

	//info("ktau_shctr_handler_def: Enter: current:%d ev:%p type:%d\n", current->pid, ev, type);

	buf = ev->ctr->cont->buf;

	//info("ktau_shctr_handler_def: buf:%p\n", buf);

	//we must check that buf->cur_index is not corrupt - that may cause us to read bad mem
	//to avoid this check maybe we should only read 1-bit of cur_index - that way we cant go wrong.
	if(buf->cur_index < 0 || buf->cur_index > 1) {
		err("ktau_shctr_handler_def: pid:%d  sym:%lx  bad cur_index:%d. ", current->pid, ev->sym, buf->cur_index);
		return -1;
	}

	wmb();

	/* - find of end of ktau_bdl_buf
	 * - then point to actual buf0 or buf1, depending on cur_index
	 * - then move the offset to point to the actual counter data
	 */
	ctr_data = ktau_shctr2ctrdata(ev->ctr, buf->cur_index);

	wmb();

	//update  the correct index
	ctr_data[ev->index].timer.count++;
	ctr_data[ev->index].timer.incl += incl;
	ctr_data[ev->index].timer.excl += excl;

	//info("ktau_shctr_handler_def: Exit: current:%d ev:%p type:%d\n", current->pid, ev, type);

	return 0;
}


/* ktau_shctr_gc:
 * Garbage collect shared-counter state on process-death-without-cleanup.
 *
 * Return:
 * On success Zero, otherwise -ve.
 *
 * Note: DONT hold lock and call this.
 */
int ktau_shctr_gc(pid_t pid, struct _ktau_prof* prof) {
	struct list_head* p = NULL, *tmp = NULL;
	ktau_trigger* this_trig = NULL;
	ktau_sh_ctr* this_ctr = NULL;
	int i = 0;

	info("ktau_shctr_gc: Enter: pid:%d prof:%p.", pid, prof);
	
	/* - Go through the task->ktau->shctr_list and for each CTR
	 *	- remove the CTR from ktau->shctr_list
	 *	- for every trigger in CTR->triggers
	 *		- remove from list (pend or htrig).
	 *		- if on htrig, then check if htrig[hash_index]
	 *		  is empty, and if so free the list_head*.
	 *		- reduce pending, if pending.
	 *	- then free all the triggers
	 * 	- de-reg this CTR from its sh-container
	 *	- free the CTR
	*/

	list_for_each_safe(p, tmp, &(prof->shctr_list)) {
		this_ctr = list_entry(p, ktau_sh_ctr, list);
		list_del(&(this_ctr->list));
		//there as many triggers as symbols
		for(i = 0; i< this_ctr->desc.no_syms; i++) {
			this_trig = &(this_ctr->triggers[i]);
			list_del(&(this_trig->list));
			if(!this_trig->pending) {
				if(list_empty(prof->htrigger_list[this_trig->hash_index])) {
					kfree(prof->htrigger_list[this_trig->hash_index]);
					prof->htrigger_list[this_trig->hash_index] = NULL;
				}
			}
		}
		kfree(this_ctr->triggers);
		this_ctr->triggers = NULL;
		unregister_ctr_shcont(this_ctr);
		kfree(this_ctr);
		prof->no_shctrs--;
	}

	//do some sanity checks.. by now pending must be empty
	if(!list_empty(&(prof->pend_evs_list))) {
		err("ktau_shctr_gc: pid:%d After GC of shctrs, pending_list non-empty. Throwing away evs.", pid);
		//what else can we do?
		return -1;
	}

	info("ktau_shctr_gc: Exit: pid:%d prof:%p.", pid, prof);
	return 0;
}


/* ktau_shcont_gc:
 * Garbage collect shared-container state on process-death-without-cleanup.
 *
 * Return:
 * On success Zero, otherwise -ve.
 *
 * Note: Assumes that container(s) not being used.
 * It will do the GC even
 * if ref_cnt of container's are non-zero.
 * So call ktau_shctr_gc before calling this.
 *
 * Note: DONT hold lock and call this.
 */
int ktau_shcont_gc(pid_t pid, struct _ktau_prof* prof) {
	int i = 0, ret = 0;
	ktau_shcont* shcont = NULL;
	
	info("ktau_shcont_gc: Enter: pid:%d prof:%p.", pid, prof);

	for(i = 0; i < KTAU_MAX_SHCONTS; i++) {
		shcont = &(prof->shconts[i]);
		if(shcont->on || (shcont->ref_cnt != 0)) {
			if(shcont->ref_cnt != 1) {
				err("ktau_shcont_gc: pid:%d Warn: shcont[%d, kid:%d].ref_cnt[%d] != 1.", pid, i, shcont->kid, shcont->ref_cnt);
				//but remove anyways
				shcont->ref_cnt = 0;
				ret = -1;
			}
			//unmap
			if(!shcont->zcb_on) {
				ktau_unmap_userbuf(pid, shcont->pages, shcont->nr_pages, shcont->orig_kaddr);
			} 
			shcont->on = 0;
			shcont->uaddr = shcont->kaddr = shcont->orig_kaddr = 0;
			shcont->nr_pages = shcont->size = shcont->next_off = 0;
			shcont->pages = shcont->buf = NULL;
		}
	}

	info("ktau_shcont_gc: Exit: pid:%d prof:%p.", pid, prof);
	return ret;
}


