/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <mt-plat/sync_write.h>
#include "mt_innercache.h"

#ifdef PERF_MEASURE
unsigned long raw_range;
unsigned long raw_setway;
#endif

/*
 * inner_dcache_flush_all: Flush (clean + invalidate) the entire L1 data cache.
 *
 * This can be used ONLY by the M4U driver!!
 * Other drivers should NOT use this function at all!!
 * Others should use DMA-mapping APIs!!
 *
 * After calling the function, the buffer should not be touched anymore.
 * And the M4U driver should then call outer_flush_all() immediately.
 * Here is the example:
 *     // Cannot touch the buffer from here.
 *     inner_dcache_flush_all();
 *     outer_flush_all();
 *     // Can touch the buffer from here.
 * If preemption occurs and the driver cannot guarantee that no other process will touch the buffer,
 * the driver should use LOCK to protect this code segment.
 */

void inner_dcache_flush_all(void)
{
	__inner_flush_dcache_all();
}
EXPORT_SYMBOL(inner_dcache_flush_all);

void inner_dcache_flush_L1(void)
{
	__inner_flush_dcache_L1();
}

void inner_dcache_flush_L2(void)
{
	__inner_flush_dcache_L2();
}

/* ARCH ARM32 */
int get_cluster_core_count(void)
{
	unsigned int cores;

	asm volatile ("MRC p15, 1, %0, c9, c0, 2\n":"=r" (cores)
		      : : "cc");

	return (cores >> 24) + 1;
}

/*
 * smp_inner_dcache_flush_all: Flush (clean + invalidate) the entire L1 data cache.
 *
 * This can be used ONLY by the M4U driver!!
 * Other drivers should NOT use this function at all!!
 * Others should use DMA-mapping APIs!!
 *
 * This is the smp version of inner_dcache_flush_all().
 * It will use IPI to do flush on all CPUs.
 * Must not call this function with disabled interrupts or from a
 * hardware interrupt handler or from a bottom half handler.
 */
void smp_inner_dcache_flush_all(void)
{
	int i, j, num_core, total_core, online_cpu;
	struct cpumask mask;
#ifdef PERF_MEASURE
	struct timespec time_stamp0, time_stamp1;
#endif

	if (in_interrupt()) {
		pr_err("Cannot invoke smp_inner_dcache_flush_all() in interrupt/softirq context\n");
		return;
	}

	get_online_cpus();
	preempt_disable();

#ifdef PERF_MEASURE
	getnstimeofday(&time_stamp0);
#endif
	on_each_cpu((smp_call_func_t) inner_dcache_flush_L1, NULL, true);

	num_core = get_cluster_core_count();
	total_core = num_possible_cpus();

	/*
	printk("In %s:%d: num_core = %d, total_core = %d\n", __func__, __LINE__, num_core, total_core);
	*/

	for (i = 0; i < total_core; i += num_core) {
		cpumask_clear(&mask);
		for (j = i; j < (i + num_core); j++) {
			/* check the online status, then set bit */
			if (cpu_online(j))
				cpumask_set_cpu(j, &mask);
		}
		online_cpu = cpumask_first_and(cpu_online_mask, &mask);
		/*
		printk("online mask = 0x%x, mask = 0x%x, id =%d\n",
			*(unsigned int *)cpu_online_mask->bits, *(unsigned int *)mask.bits, online_cpu);
		*/
		smp_call_function_single(online_cpu, (smp_call_func_t) inner_dcache_flush_L2, NULL,
					 true);

	}
#ifdef PERF_MEASURE
	getnstimeofday(&time_stamp1);
	raw_setway = 1000000000 * (time_stamp1.tv_sec - time_stamp0.tv_sec) +
		(time_stamp1.tv_nsec - time_stamp0.tv_nsec);
#endif
	preempt_enable();
	put_online_cpus();
}
EXPORT_SYMBOL(smp_inner_dcache_flush_all);

#ifdef CONFIG_MTK_CACHE_FLUSH_RANGE_PARALLEL

#include <linux/of_address.h>
#include <linux/sizes.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/scatterlist.h>
#include <linux/delay.h>
#include <asm/spinlock.h>

int (*ion_sync_kernel_func)(unsigned long start, size_t size,
		unsigned int sync_type);

struct smp_sync_sg_list_arg {
	struct sg_table *table;
	unsigned int sync_type;
	int ret_value;
	int npages;
};

struct vm_struct *cache_flush_vm_struct[NR_CPUS];
static void *cache_flush_map_page_va(struct vm_struct *vm, struct page *page)
{
	int ret;
	struct page **ppPage = &page;

	ret = map_vm_area(vm, PAGE_KERNEL, ppPage);
	if (ret) {
		pr_err("error to map page (err %d)\n", ret);
		return NULL;
	}
	return vm->addr;
}

static void cache_flush_unmap_page_va(struct vm_struct *vm)
{
	unmap_kernel_range((unsigned long) vm->addr, PAGE_SIZE);
}

/* must be involked after we got hotplug.lock */
static void _smp_inner_dcache_flush_all(void)
{
	int i, j, num_core, total_core, online_cpu;
	struct cpumask mask;
#ifdef PERF_MEASURE
	struct timespec time_stamp0, time_stamp1;
#endif

	preempt_disable();

#ifdef PERF_MEASURE
	getnstimeofday(&time_stamp0);
#endif
	on_each_cpu((smp_call_func_t) inner_dcache_flush_L1, NULL, true);

	num_core = get_cluster_core_count();
	total_core = num_possible_cpus();

	for (i = 0; i < total_core; i += num_core) {
		cpumask_clear(&mask);
		for (j = i; j < (i + num_core); j++) {
			/* check the online status, then set bit */
			if (cpu_online(j))
				cpumask_set_cpu(j, &mask);
		}
		online_cpu = cpumask_first_and(cpu_online_mask, &mask);
		smp_call_function_single(online_cpu, (smp_call_func_t) inner_dcache_flush_L2, NULL,
					 true);

	}
#ifdef PERF_MEASURE
	getnstimeofday(&time_stamp1);
	raw_setway = 1000000000 * (time_stamp1.tv_sec - time_stamp0.tv_sec) +
		(time_stamp1.tv_nsec - time_stamp0.tv_nsec);
#endif
	preempt_enable();
}

typedef enum {
	FAILED,
	STARTING,
	FLUSHING,
	WAITING_FOR_OTHERS,
} flush_state;

static struct scatterlist *sg_sync;
static flush_state f_state;

spinlock_t sg_flush_lock;
spinlock_t smp_cache_flush_lock;
atomic_t nr_done_pages;

void _flush_sg_list(struct smp_sync_sg_list_arg *args)
{
	unsigned long start = -1;
	unsigned int this_cpu = smp_processor_id();
	unsigned int sync_type = args->sync_type;
	int npages_this_entry, j;
	struct sg_table *table = (struct sg_table *) args->table;
	struct page *page;

	npages_this_entry = -1;
	page = NULL;

	do {
		/* update sg_sync and f_state */
		spin_lock(&sg_flush_lock);
		switch (f_state) {
		case STARTING:
			/* get the first scatter list */
			sg_sync = (table->sgl);
			/* update state */
			f_state = FLUSHING;
			break;
		case FLUSHING:
			sg_sync = sg_next(sg_sync);
			break;
		default:
			break;
		}

		if (sg_sync != NULL) {
			npages_this_entry = PAGE_ALIGN(sg_sync->length) / PAGE_SIZE;
			page = sg_page(sg_sync);
		} else {
			/* sg_sync == NULL when reaching the end of sg list */
			if (f_state != FAILED)
				f_state = WAITING_FOR_OTHERS;
		}
		spin_unlock(&sg_flush_lock);

		switch (f_state) {
		case FLUSHING:
			if (npages_this_entry == -1 && page == NULL)
				break;

			/* flush pages */
			for (j = 0; j < npages_this_entry; j++) {
				start = (unsigned long)
					cache_flush_map_page_va(cache_flush_vm_struct[this_cpu], page++);
				if (unlikely(IS_ERR_OR_NULL((void *) start))) {
					pr_err("[smp cache flush] cannot do cache sync: ret=%lu\n", start);
					args->ret_value = -EFAULT;
					spin_lock(&sg_flush_lock);
					f_state = FAILED;
					spin_unlock(&sg_flush_lock);
					return;
				}
				/*
				pr_err("[smp cache flush] __smp_sync_sg_list: start = 0x%lx, size = 0x%x\n",
						start, (unsigned int) PAGE_SIZE);
				*/

				if (likely(ion_sync_kernel_func))
					ion_sync_kernel_func(start, PAGE_SIZE, sync_type);
				else
					pr_err("[smp cache flush] ion_sync_kernel_func is NULL\n");

				cache_flush_unmap_page_va(cache_flush_vm_struct[this_cpu]);
			}
			/* atomic increment */
			atomic_add(npages_this_entry, &nr_done_pages);
			break;
		default:
			break;
		}
	} while (f_state == FLUSHING);
}

int smp_sync_cache_by_cpu_pool_ion(struct sg_table *table, unsigned int sync_type, int npages)
{
	struct smp_sync_sg_list_arg smp_call_args;
	struct cpumask mask;
	unsigned long flags, cpu = 0, largest_affinity_cpu = -1, this_cpu;
#ifdef PERF_MEASURE
	struct timespec time_stamp0, time_stamp1;
#endif

	spin_lock(&smp_cache_flush_lock);
	preempt_disable();

#ifdef PERF_MEASURE
	getnstimeofday(&time_stamp0);
#endif

	atomic_set(&nr_done_pages, 0);
	spin_lock_irqsave(&sg_flush_lock, flags);
	f_state = STARTING;
	spin_unlock_irqrestore(&sg_flush_lock, flags);

	smp_call_args.table = table;
	smp_call_args.npages = npages;
	smp_call_args.sync_type = sync_type;
	smp_call_args.ret_value = 0;

	/* mask = {cpu_online_mask} - {the other online cpus with largest affinity} */
	this_cpu = smp_processor_id();
	cpumask_copy(&mask, cpu_online_mask);

	for_each_cpu(cpu, &mask)
		if (cpu != this_cpu)
			largest_affinity_cpu = cpu;
	if (largest_affinity_cpu != -1)
		cpumask_clear_cpu(largest_affinity_cpu, &mask);

	/*
	on_each_cpu((smp_call_func_t) _flush_sg_list, (void *)&smp_call_args, false);
	*/
	on_each_cpu_mask(&mask, (smp_call_func_t) _flush_sg_list, (void *)&smp_call_args, false);

	/* this cpu waits for the flush complete before returns */
	while ((atomic_read(&nr_done_pages) < npages) && (f_state != FAILED))
		;

#ifdef PERF_MEASURE
	getnstimeofday(&time_stamp1);
	raw_range = 1000000000 * (time_stamp1.tv_sec - time_stamp0.tv_sec) +
		(time_stamp1.tv_nsec - time_stamp0.tv_nsec);
#endif
	preempt_enable();
	spin_unlock(&smp_cache_flush_lock);
	return smp_call_args.ret_value;
}

int mt_smp_cache_flush(struct sg_table *table, unsigned int sync_type, int npages)
{
	int ret = -1;
	bool get_lock = false;
	long timeout = CACHE_FLUSH_TIMEOUT;

	if (in_interrupt()) {
		pr_err("Cannot invoke mt_smp_cache_flush() in interrupt/softirq context\n");
		return -EFAULT;
	}

	/* trylock for timeout nsec */
	while (!(get_lock = try_get_online_cpus()) && (timeout-- > 0))
		udelay(1);

	if (get_lock) {
		/* call flush all by set/way if we got lock */
		_smp_inner_dcache_flush_all();
		put_online_cpus();
		return CACHE_FLUSH_BY_SETWAY;
	}

	ret = smp_sync_cache_by_cpu_pool_ion(table, sync_type, npages);
	return (ret >= 0) ? CACHE_FLUSH_BY_MVA : ret;
}
EXPORT_SYMBOL(mt_smp_cache_flush);

/* === m4u version === */
#include <asm/cacheflush.h>

struct smp_sync_range_arg {
	unsigned int sync_type;
	unsigned long va_start;
	unsigned long va_end;
	unsigned long size;
	int ret_value;
};

#define NR_PAGES_PER_FLUSH 2

atomic_t done_range;
unsigned long flush_range_head;

void _flush_range(struct smp_sync_range_arg *args)
{
	unsigned long start = 0;
	unsigned long size = 0;

	do {
		/* update sg_sync and f_state */
		spin_lock(&sg_flush_lock);
		switch (f_state) {
		case STARTING:
			/* update state */
			f_state = FLUSHING;
			break;
		case FLUSHING:
			start = flush_range_head;
			if (flush_range_head + (NR_PAGES_PER_FLUSH * PAGE_SIZE) > args->va_end) {
				size = (NR_PAGES_PER_FLUSH * PAGE_SIZE);
				flush_range_head += (NR_PAGES_PER_FLUSH * PAGE_SIZE);
			} else {
				size = args->va_end - flush_range_head;
				flush_range_head = args->va_end + 1;
				if (f_state != FAILED)
					f_state = WAITING_FOR_OTHERS;
			}
			break;
		default:
			break;
		}
		spin_unlock(&sg_flush_lock);

		switch (f_state) {
		case FLUSHING:
			if ((size == 0) || (start <= args->va_start) || (start > args->va_end))
				break;

			/*
			pr_err("[smp cache flush] __flush_range: start = 0x%lx, size = 0x%x\n",
					start, (unsigned int) size);
			*/
			dmac_flush_range((void *)start, (void *) start + size);

			/* atomic increment */
			atomic_add(size, &done_range);
			break;
		default:
			break;
		}
	} while (f_state == FLUSHING);
}

int smp_sync_cache_by_cpu_pool_m4u(const void *va, const unsigned long size)
{
	struct smp_sync_range_arg smp_call_args;
	struct cpumask mask;
	unsigned long flags, cpu = 0, largest_affinity_cpu = -1, this_cpu;
#ifdef PERF_MEASURE
	struct timespec time_stamp0, time_stamp1;
#endif

	spin_lock(&smp_cache_flush_lock);
	preempt_disable();

#ifdef PERF_MEASURE
	getnstimeofday(&time_stamp0);
#endif

	atomic_set(&done_range, 0);
	spin_lock_irqsave(&sg_flush_lock, flags);
	f_state = STARTING;
	spin_unlock_irqrestore(&sg_flush_lock, flags);

	smp_call_args.va_start = (unsigned long)va;
	smp_call_args.va_end = (unsigned long)va + size - 1;
	smp_call_args.size = size;
	smp_call_args.ret_value = 0;

	/* mask = {cpu_online_mask} - {the other online cpus with largest affinity} */
	this_cpu = smp_processor_id();
	cpumask_copy(&mask, cpu_online_mask);

	for_each_cpu(cpu, &mask)
		if (cpu != this_cpu)
			largest_affinity_cpu = cpu;
	if (largest_affinity_cpu != -1)
		cpumask_clear_cpu(largest_affinity_cpu, &mask);

	/*
	on_each_cpu((smp_call_func_t) _flush_range, (void *)&smp_call_args, false);
	*/
	on_each_cpu_mask(&mask, (smp_call_func_t) _flush_range, (void *)&smp_call_args, false);

	/* this cpu waits for the flush complete before returns */
	while ((atomic_read(&done_range) < size) && (f_state != FAILED))
		;

#ifdef PERF_MEASURE
	getnstimeofday(&time_stamp1);
	raw_range = 1000000000 * (time_stamp1.tv_sec - time_stamp0.tv_sec) +
		(time_stamp1.tv_nsec - time_stamp0.tv_nsec);
#endif
	preempt_enable();
	spin_unlock(&smp_cache_flush_lock);
	return smp_call_args.ret_value;
}


int mt_smp_cache_flush_m4u(const void *va, const unsigned long size)
{
	int ret = -1;
	bool get_lock = false;
	long timeout = CACHE_FLUSH_TIMEOUT;

	if (in_interrupt()) {
		pr_err("Cannot invoke mt_smp_cache_flush() in interrupt/softirq context\n");
		return -EFAULT;
	}

	/* trylock for timeout nsec */
	while (!(get_lock = try_get_online_cpus()) && (timeout-- > 0))
		udelay(1);

	if (get_lock) {
		/* call flush all by set/way if we got lock */
		_smp_inner_dcache_flush_all();
		put_online_cpus();
		return CACHE_FLUSH_BY_SETWAY;
	}

	ret = smp_sync_cache_by_cpu_pool_m4u(va, size);
	return (ret >= 0) ? CACHE_FLUSH_BY_MVA : ret;
}
EXPORT_SYMBOL(mt_smp_cache_flush_m4u);

static int __init cache_flush_module(void)
{
	int i;

	/* initialize vm_struct for each cpu to map/unmap vm */
	for (i = 0; i <= num_possible_cpus()-1; ++i) {
		cache_flush_vm_struct[i] = get_vm_area(PAGE_SIZE, VM_ALLOC);
		WARN_ON(!cache_flush_vm_struct[i]);
	}

	spin_lock_init(&smp_cache_flush_lock);
	spin_lock_init(&sg_flush_lock);

	return 0;
}
module_init(cache_flush_module);
#endif
