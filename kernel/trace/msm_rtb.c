// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2018, The Linux Foundation. All rights reserved.
 */

#include <linux/atomic.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/atomic.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <asm-generic/sizes.h>
#include <linux/msm_rtb.h>
#include <asm/timex.h>
#include <soc/qcom/minidump.h>

#define SENTINEL_BYTE_1 0xFF
#define SENTINEL_BYTE_2 0xAA
#define SENTINEL_BYTE_3 0xFF

#define RTB_COMPAT_STR	"qcom,msm-rtb"

/* Write
 * 1) 3 bytes sentinel
 * 2) 1 bytes of log type
 * 3) 8 bytes of where the caller came from
 * 4) 4 bytes index
 * 4) 8 bytes extra data from the caller
 * 5) 8 bytes of timestamp
 * 6) 8 bytes of cyclecount
 *
 * Total = 40 bytes.
 */
struct msm_rtb_layout {
	unsigned char sentinel[3];
	unsigned char log_type;
	uint32_t idx;
	uint64_t caller;
	uint64_t data;
	uint64_t timestamp;
	uint64_t cycle_count;
} __attribute__ ((__packed__));


struct msm_rtb_state {
	struct msm_rtb_layout *rtb;
	phys_addr_t phys;
	int nentries;
	int size;
	int enabled;
	int initialized;
	uint32_t filter;
	int step_size;
};

#if defined(CONFIG_QCOM_RTB_SEPARATE_CPUS)
DEFINE_PER_CPU(atomic_t, msm_rtb_idx_cpu);
#else
static atomic_t msm_rtb_idx;
#endif

static struct msm_rtb_state msm_rtb = {
	.filter = 1 << LOGK_LOGBUF,
	.enabled = 1,
};

module_param_named(filter, msm_rtb.filter, uint, 0644);
module_param_named(enable, msm_rtb.enabled, int, 0644);

static int msm_rtb_panic_notifier(struct notifier_block *this,
					unsigned long event, void *ptr)
{
	msm_rtb.enabled = 0;
	return NOTIFY_DONE;
}

static struct notifier_block msm_rtb_panic_blk = {
	.notifier_call  = msm_rtb_panic_notifier,
	.priority = INT_MAX,
};

int notrace msm_rtb_event_should_log(enum logk_event_type log_type)
{
	return msm_rtb.initialized && msm_rtb.enabled &&
		((1 << (log_type & ~LOGTYPE_NOPC)) & msm_rtb.filter);
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

static void msm_rtb_write_caller(uint64_t caller, struct msm_rtb_layout *start)
{
	start->caller = caller;
}

static void msm_rtb_write_idx(uint32_t idx,
				struct msm_rtb_layout *start)
{
	start->idx = idx;
}

static void msm_rtb_write_data(uint64_t data, struct msm_rtb_layout *start)
{
	start->data = data;
}

static void msm_rtb_write_timestamp(struct msm_rtb_layout *start)
{
	start->timestamp = sched_clock();
}

static void msm_rtb_write_cyclecount(struct msm_rtb_layout *start)
{
	start->cycle_count = get_cycles();
}

static void uncached_logk_pc_idx(enum logk_event_type log_type, uint64_t caller,
				 uint64_t data, int idx)
{
	struct msm_rtb_layout *start;

	start = &msm_rtb.rtb[idx & (msm_rtb.nentries - 1)];

	msm_rtb_emit_sentinel(start);
	msm_rtb_write_type(log_type, start);
	msm_rtb_write_caller(caller, start);
	msm_rtb_write_idx(idx, start);
	msm_rtb_write_data(data, start);
	msm_rtb_write_timestamp(start);
	msm_rtb_write_cyclecount(start);
	mb();

}

static void uncached_logk_timestamp(int idx)
{
	unsigned long long timestamp;

	timestamp = sched_clock();
	uncached_logk_pc_idx(LOGK_TIMESTAMP|LOGTYPE_NOPC,
			(uint64_t)lower_32_bits(timestamp),
			(uint64_t)upper_32_bits(timestamp), idx);
}

#if defined(CONFIG_QCOM_RTB_SEPARATE_CPUS)
static int msm_rtb_get_idx(void)
{
	int cpu, i, offset;
	atomic_t *index;

	/*
	 * ideally we would use get_cpu but this is a close enough
	 * approximation for our purposes.
	 */
	cpu = raw_smp_processor_id();

	index = &per_cpu(msm_rtb_idx_cpu, cpu);

	i = atomic_add_return(msm_rtb.step_size, index);
	i -= msm_rtb.step_size;

	/* Check if index has wrapped around */
	offset = (i & (msm_rtb.nentries - 1)) -
		 ((i - msm_rtb.step_size) & (msm_rtb.nentries - 1));
	if (offset < 0) {
		uncached_logk_timestamp(i);
		i = atomic_add_return(msm_rtb.step_size, index);
		i -= msm_rtb.step_size;
	}

	return i;
}
#else
static int msm_rtb_get_idx(void)
{
	int i, offset;

	i = atomic_inc_return(&msm_rtb_idx);
	i--;

	/* Check if index has wrapped around */
	offset = (i & (msm_rtb.nentries - 1)) -
		 ((i - 1) & (msm_rtb.nentries - 1));
	if (offset < 0) {
		uncached_logk_timestamp(i);
		i = atomic_inc_return(&msm_rtb_idx);
		i--;
	}

	return i;
}
#endif

int notrace uncached_logk_pc(enum logk_event_type log_type, void *caller,
				void *data)
{
	int i;

	if (!msm_rtb_event_should_log(log_type))
		return 0;

	i = msm_rtb_get_idx();
	uncached_logk_pc_idx(log_type, (uint64_t)((unsigned long) caller),
				(uint64_t)((unsigned long) data), i);

	return 1;
}
EXPORT_SYMBOL(uncached_logk_pc);

noinline int notrace uncached_logk(enum logk_event_type log_type, void *data)
{
	return uncached_logk_pc(log_type, __builtin_return_address(0), data);
}
EXPORT_SYMBOL(uncached_logk);

static int msm_rtb_probe(struct platform_device *pdev)
{
	struct msm_rtb_platform_data *d = pdev->dev.platform_data;
	struct md_region md_entry;
#if defined(CONFIG_QCOM_RTB_SEPARATE_CPUS)
	unsigned int cpu;
#endif
	int ret;

	if (!pdev->dev.of_node) {
		msm_rtb.size = d->size;
	} else {
		u64 size;
		struct device_node *pnode;

		pnode = of_parse_phandle(pdev->dev.of_node,
						"linux,contiguous-region", 0);
		if (pnode != NULL) {
			const u32 *addr;

			addr = of_get_address(pnode, 0, &size, NULL);
			if (!addr) {
				of_node_put(pnode);
				return -EINVAL;
			}
			of_node_put(pnode);
		} else {
			ret = of_property_read_u32(pdev->dev.of_node,
					"qcom,rtb-size",
					(u32 *)&size);
			if (ret < 0)
				return ret;

		}

		msm_rtb.size = size;
	}

	if (msm_rtb.size <= 0 || msm_rtb.size > SZ_1M)
		return -EINVAL;

	msm_rtb.rtb = dma_alloc_coherent(&pdev->dev, msm_rtb.size,
						&msm_rtb.phys,
						GFP_KERNEL);

	if (!msm_rtb.rtb)
		return -ENOMEM;

	msm_rtb.nentries = msm_rtb.size / sizeof(struct msm_rtb_layout);

	/* Round this down to a power of 2 */
	msm_rtb.nentries = __rounddown_pow_of_two(msm_rtb.nentries);

	memset(msm_rtb.rtb, 0, msm_rtb.size);

	strlcpy(md_entry.name, "KRTB_BUF", sizeof(md_entry.name));
	md_entry.virt_addr = (uintptr_t)msm_rtb.rtb;
	md_entry.phys_addr = msm_rtb.phys;
	md_entry.size = msm_rtb.size;
	if (msm_minidump_add_region(&md_entry))
		pr_info("Failed to add RTB in Minidump\n");

#if defined(CONFIG_QCOM_RTB_SEPARATE_CPUS)
	for_each_possible_cpu(cpu) {
		atomic_t *a = &per_cpu(msm_rtb_idx_cpu, cpu);

		atomic_set(a, cpu);
	}
	msm_rtb.step_size = num_possible_cpus();
#else
	atomic_set(&msm_rtb_idx, 0);
	msm_rtb.step_size = 1;
#endif

	atomic_notifier_chain_register(&panic_notifier_list,
						&msm_rtb_panic_blk);
	msm_rtb.initialized = 1;
	return 0;
}

static const struct of_device_id msm_match_table[] = {
	{.compatible = RTB_COMPAT_STR},
	{},
};

static struct platform_driver msm_rtb_driver = {
	.probe = msm_rtb_probe,
	.driver         = {
		.name = "msm_rtb",
		.owner = THIS_MODULE,
		.of_match_table = msm_match_table
	},
};
module_platform_driver(msm_rtb_driver);
