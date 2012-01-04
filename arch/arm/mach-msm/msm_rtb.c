/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/atomic.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/memory_alloc.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <asm/io.h>
#include <asm-generic/sizes.h>
#include <mach/memory.h>
#include <mach/msm_rtb.h>
#include <mach/system.h>

#define SENTINEL_BYTE_1 0xFF
#define SENTINEL_BYTE_2 0xAA
#define SENTINEL_BYTE_3 0xFF

/* Write
 * 1) 3 bytes sentinel
 * 2) 1 bytes of log type
 * 3) 4 bytes of where the caller came from
 * 4) 4 bytes index
 * 4) 4 bytes extra data from the caller
 *
 * Total = 16 bytes.
 */
struct msm_rtb_layout {
	unsigned char sentinel[3];
	unsigned char log_type;
	void *caller;
	unsigned long idx;
	void *data;
} __attribute__ ((__packed__));


struct msm_rtb_state {
	struct msm_rtb_layout *rtb;
	unsigned long phys;
	int nentries;
	int size;
	int enabled;
	uint32_t filter;
	int step_size;
};

#if defined(CONFIG_MSM_RTB_SEPARATE_CPUS)
DEFINE_PER_CPU(atomic_t, msm_rtb_idx_cpu);
#else
static atomic_t msm_rtb_idx;
#endif

struct msm_rtb_state msm_rtb = {
	.size = SZ_1M,
};

module_param_named(filter, msm_rtb.filter, uint, 0644);
module_param_named(enable, msm_rtb.enabled, int, 0644);

int msm_rtb_event_should_log(enum logk_event_type log_type)
{
	return msm_rtb.enabled &&
		((1 << log_type) & msm_rtb.filter);
}
EXPORT_SYMBOL(msm_rtb_event_should_log);

static void msm_rtb_emit_sentinel(struct msm_rtb_layout *start)
{
	start->sentinel[0] = SENTINEL_BYTE_1;
	start->sentinel[1] = SENTINEL_BYTE_2;
	start->sentinel[2] = SENTINEL_BYTE_3;
}

static void msm_rtb_write_type(enum logk_event_type log_type,
			struct msm_rtb_layout *start)
{
	start->log_type = (char)log_type;
}

static void msm_rtb_write_caller(void *caller, struct msm_rtb_layout *start)
{
	start->caller = caller;
}

static void msm_rtb_write_idx(unsigned long idx,
				struct msm_rtb_layout *start)
{
	start->idx = idx;
}

static void msm_rtb_write_data(void *data, struct msm_rtb_layout *start)
{
	start->data = data;
}

static int __init msm_rtb_set_buffer_size(char *p)
{
	int s;

	s = memparse(p, NULL);
	msm_rtb.size = ALIGN(s, SZ_4K);
	return 0;
}
early_param("msm_rtb_size", msm_rtb_set_buffer_size);

#if defined(CONFIG_MSM_RTB_SEPARATE_CPUS)
static int msm_rtb_get_idx(void)
{
	int cpu, i;
	atomic_t *index;

	/*
	 * ideally we would use get_cpu but this is a close enough
	 * approximation for our purposes.
	 */
	cpu = raw_smp_processor_id();

	index = &per_cpu(msm_rtb_idx_cpu, cpu);

	i = atomic_add_return(msm_rtb.step_size, index);
	i -= msm_rtb.step_size;

	return i;
}
#else
static int msm_rtb_get_idx(void)
{
	int i;

	i = atomic_inc_return(&msm_rtb_idx);
	i--;

	return i;
}
#endif

int uncached_logk_pc(enum logk_event_type log_type, void *caller,
				void *data)
{
	int i;
	struct msm_rtb_layout *start;

	if (!msm_rtb_event_should_log(log_type))
		return 0;

	i = msm_rtb_get_idx();

	start = &msm_rtb.rtb[i & (msm_rtb.nentries - 1)];

	msm_rtb_emit_sentinel(start);
	msm_rtb_write_type(log_type, start);
	msm_rtb_write_caller(caller, start);
	msm_rtb_write_idx(i, start);
	msm_rtb_write_data(data, start);
	mb();

	return 1;
}
EXPORT_SYMBOL(uncached_logk_pc);

noinline int uncached_logk(enum logk_event_type log_type, void *data)
{
	return uncached_logk_pc(log_type, __builtin_return_address(0), data);
}
EXPORT_SYMBOL(uncached_logk);

int msm_rtb_init(void)
{
#if defined(CONFIG_MSM_RTB_SEPARATE_CPUS)
	unsigned int cpu;
#endif

	if (msm_rtb.size <= 0 || msm_rtb.size > SZ_1M)
		return -EINVAL;

	/*
	 * The ioremap call is made separately to store the physical
	 * address of the buffer. This is necessary for cases where
	 * the only way to access the buffer is a physical address.
	 */
	msm_rtb.phys = allocate_contiguous_ebi_nomap(msm_rtb.size, SZ_4K);

	if (!msm_rtb.phys)
		return -ENOMEM;

	msm_rtb.rtb = ioremap(msm_rtb.phys, msm_rtb.size);

	if (!msm_rtb.rtb) {
		free_contiguous_memory_by_paddr(msm_rtb.phys);
		return -ENOMEM;
	}

	msm_rtb.nentries = msm_rtb.size / sizeof(struct msm_rtb_layout);

	/* Round this down to a power of 2 */
	msm_rtb.nentries = __rounddown_pow_of_two(msm_rtb.nentries);

	memset(msm_rtb.rtb, 0, msm_rtb.size);


#if defined(CONFIG_MSM_RTB_SEPARATE_CPUS)
	for_each_possible_cpu(cpu) {
		atomic_t *a = &per_cpu(msm_rtb_idx_cpu, cpu);
		atomic_set(a, cpu);
	}
	msm_rtb.step_size = num_possible_cpus();
#else
	atomic_set(&msm_rtb_idx, 0);
	msm_rtb.step_size = 1;
#endif


	msm_rtb.enabled = 1;
	return 0;
}
module_init(msm_rtb_init)
