/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_VM_EVENT_H
#define _LINUX_VM_EVENT_H

#include <linux/types.h>
#include <linux/percpu.h>
#include <linux/mmzone.h>
#include <linux/vm_event_item.h>
#include <linux/atomic.h>

#ifdef CONFIG_VM_EVENT_COUNTERS
/*
 * Light weight per cpu counter implementation.
 *
 * Counters should only be incremented and no critical kernel component
 * should rely on the counter values.
 *
 * Counters are handled completely inline. On many platforms the code
 * generated will simply be the increment of a global address.
 */

struct vm_event_state {
	unsigned long event[NR_VM_EVENT_ITEMS];
};

DECLARE_PER_CPU(struct vm_event_state, vm_event_states);

/*
 * vm counters are allowed to be racy. Use raw_cpu_ops to avoid the
 * local_irq_disable overhead.
 */
static inline void __count_vm_event(enum vm_event_item item)
{
	raw_cpu_inc(vm_event_states.event[item]);
}

static inline void count_vm_event(enum vm_event_item item)
{
	this_cpu_inc(vm_event_states.event[item]);
}

static inline void __count_vm_events(enum vm_event_item item, long delta)
{
	raw_cpu_add(vm_event_states.event[item], delta);
}

static inline void count_vm_events(enum vm_event_item item, long delta)
{
	this_cpu_add(vm_event_states.event[item], delta);
}

extern void all_vm_events(unsigned long *);

extern void vm_events_fold_cpu(int cpu);

#else

/* Disable counters */
static inline void count_vm_event(enum vm_event_item item)
{
}
static inline void count_vm_events(enum vm_event_item item, long delta)
{
}
static inline void __count_vm_event(enum vm_event_item item)
{
}
static inline void __count_vm_events(enum vm_event_item item, long delta)
{
}
static inline void all_vm_events(unsigned long *ret)
{
}
static inline void vm_events_fold_cpu(int cpu)
{
}

#endif /* CONFIG_VM_EVENT_COUNTERS */

#ifdef CONFIG_SPECULATIVE_PAGE_FAULT_STATS
#define count_vm_spf_event(x) count_vm_event(x)
#else
#define count_vm_spf_event(x) do {} while (0)
#endif

#ifdef CONFIG_NUMA_BALANCING
#define count_vm_numa_event(x)     count_vm_event(x)
#define count_vm_numa_events(x, y) count_vm_events(x, y)
#else
#define count_vm_numa_event(x) do {} while (0)
#define count_vm_numa_events(x, y) do { (void)(y); } while (0)
#endif /* CONFIG_NUMA_BALANCING */

#ifdef CONFIG_DEBUG_TLBFLUSH
#define count_vm_tlb_event(x)	   count_vm_event(x)
#define count_vm_tlb_events(x, y)  count_vm_events(x, y)
#else
#define count_vm_tlb_event(x)     do {} while (0)
#define count_vm_tlb_events(x, y) do { (void)(y); } while (0)
#endif

#ifdef CONFIG_DEBUG_VM_VMACACHE
#define count_vm_vmacache_event(x) count_vm_event(x)
#else
#define count_vm_vmacache_event(x) do {} while (0)
#endif

#define __count_zid_vm_events(item, zid, delta) \
	__count_vm_events(item##_NORMAL - ZONE_NORMAL + zid, delta)

#endif /* _LINUX_VM_EVENT_H */
