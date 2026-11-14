// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023 Google LLC
 */

#include <nvhe/mm.h>
#include <nvhe/mem_protect.h>
#include <nvhe/trace.h>

#include <nvhe/define_events.h>

extern struct hyp_event_id __hyp_event_ids_start[];
extern struct hyp_event_id __hyp_event_ids_end[];

#define MAX_MOD_EVENTS 16 /* Max number of modules with events */

static atomic_t num_hyp_event_mods = ATOMIC_INIT(0);
static DEFINE_HYP_SPINLOCK(hyp_mod_events_lock);
static struct {
	struct {
		struct hyp_event_id	*start;
		struct hyp_event_id	*end;
	} event_ids;
	struct {
		unsigned long		*start;
		unsigned long		*end;
		unsigned long		offset_idx;
		void			*tramp;
	} funcs;
} hyp_mod_events[MAX_MOD_EVENTS];

#ifdef CONFIG_PKVM_FTRACE
int __pkvm_sync_ftrace(unsigned long host_funcs_pg)
{
	unsigned long *funcs_pg = (unsigned long *)kern_hyp_va(host_funcs_pg);
	int mod, nr_mods = atomic_read(&num_hyp_event_mods);
	u64 pfn = hyp_virt_to_pfn(funcs_pg);
	void *func;
	int ret;

	ret = __pkvm_host_donate_hyp(pfn, 1);
	if (ret)
		return ret;

	/* Repurpose this lock to serialize syncs */
	hyp_spin_lock(&hyp_mod_events_lock);

	/* Look into core */
	func = hyp_ftrace_find_host_func(*funcs_pg, NULL, NULL, 0);
	if (func)
		funcs_pg = hyp_ftrace_sync(funcs_pg, func, NULL, 0, NULL);

	/* Look into modules */
	for (mod = 0; mod < nr_mods; mod++) {
		unsigned long *start = hyp_mod_events[mod].funcs.start;
		unsigned long *end = hyp_mod_events[mod].funcs.end;
		unsigned long offset_idx = hyp_mod_events[mod].funcs.offset_idx;
		void *tramp = hyp_mod_events[mod].funcs.tramp;

		if (!funcs_pg)
			break;

		func = hyp_ftrace_find_host_func(*funcs_pg, start, end,
						 offset_idx);
		if (func)
			funcs_pg = hyp_ftrace_sync(funcs_pg, func, end,
						  offset_idx, tramp);
	}

	hyp_spin_unlock(&hyp_mod_events_lock);

	WARN_ON(__pkvm_hyp_donate_host(pfn, 1));

	return funcs_pg ? -EINVAL : 0;
}

int __pkvm_disable_ftrace(void)
{
	int mod, nr_mods = atomic_read(&num_hyp_event_mods);

	hyp_ftrace_disable(NULL, NULL);

	for (mod = 0; mod < nr_mods; mod++)
		hyp_ftrace_disable(hyp_mod_events[mod].funcs.start,
				   hyp_mod_events[mod].funcs.end);

	return 0;
}
#endif

static void hyp_set_key(atomic_t *key, int val)
{
	atomic_t *__key = hyp_fixmap_map(__pkvm_private_range_pa(key));

	atomic_set(__key, val);
	hyp_fixmap_unmap();
}

static bool __try_set_event(unsigned short id, bool enable,
			    struct hyp_event_id *event_id,
			    struct hyp_event_id *end)
{
	atomic_t *enable_key;

	for (; event_id < end; event_id++) {
		if (event_id->id != id)
			continue;

		enable_key = (atomic_t *)event_id->data;
		hyp_set_key(enable_key, enable);

		return true;
	}

	return false;
}

static bool try_set_event(unsigned short id, bool enable)
{
	return __try_set_event(id, enable, __hyp_event_ids_start,
			       __hyp_event_ids_end);
}

static bool try_set_mod_event(unsigned short id, bool enable)
{
	int i, nr_mods;

	/*
	 * Order access between num_hyp_event_mods and hyp_mod_events.
	 * Paired with register_hyp_event_ids()
	 */
	nr_mods = atomic_read_acquire(&num_hyp_event_mods);

	for (i = 0; i < nr_mods; i++) {
		if (__try_set_event(id, enable,
				    hyp_mod_events[i].event_ids.start,
				    hyp_mod_events[i].event_ids.end))
			return true;
	}

	return false;
}

int register_hyp_mod_events(void *event_ids, size_t nr_events,
			    void *funcs, void *funcs_end,
			    void *tramp,
			    size_t hyp_kern_offset)
{
	size_t nr_funcs = funcs_end - funcs;
	int mod, ret = -ENOMEM;

	if (!nr_events && !nr_funcs)
		return 0;

	hyp_spin_lock(&hyp_mod_events_lock);

	mod = atomic_read(&num_hyp_event_mods);
	if (mod < MAX_MOD_EVENTS) {
		hyp_mod_events[mod].event_ids.start =
			(struct hyp_event_id *)event_ids;
		hyp_mod_events[mod].event_ids.end =
			(struct hyp_event_id *)event_ids + nr_events;
		hyp_mod_events[mod].funcs.start = funcs;
		hyp_mod_events[mod].funcs.end = funcs_end;
		hyp_mod_events[mod].funcs.tramp = tramp;

		/*
		 * Order access between num_hyp_event_mods and hyp_mod_events.
		 * Paired with try_set_mod_event()
		 */
		atomic_set_release(&num_hyp_event_mods, mod + 1);
		ret = 0;
	}

	hyp_spin_unlock(&hyp_mod_events_lock);

	if (!ret) {
		ret = hyp_ftrace_setup(funcs, funcs_end, hyp_kern_offset, tramp);
		if (ret >= 0)
			hyp_mod_events[mod].funcs.offset_idx = ret;
	}

	return ret;
}

int __pkvm_enable_event(unsigned short id, bool enable)
{
	if (try_set_event(id, enable))
		return 0;

	if (try_set_mod_event(id, enable))
		return 0;

	return -EINVAL;
}
