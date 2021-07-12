// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2019, 2021, The Linux Foundation. All rights reserved.
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
#include <linux/sizes.h>
#include <linux/msm_rtb.h>
#include <asm/timex.h>
#include <soc/qcom/minidump.h>
#include <linux/interrupt.h>
#include <trace/events/sched.h>
#include <trace/events/irq.h>
#include <trace/events/rwmmio.h>

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

struct rtb_idx {
	atomic_t idx;
	char pad[L1_CACHE_BYTES - sizeof(atomic_t)];
};

struct msm_rtb_state {
	struct rtb_idx msm_rtb_idx[NR_CPUS];
	struct msm_rtb_layout *rtb;
	phys_addr_t phys;
	int nentries;
	int size;
	int enabled;
	int initialized;
	uint32_t filter;
	int step_size;
};

static struct msm_rtb_state *msm_rtb_ptr;

static uint32_t filter = 1 << LOGK_LOGBUF;
static int enabled = 1;
module_param_named(filter, filter, uint, 0644);
module_param_named(enable, enabled, int, 0644);

static int msm_rtb_panic_notifier(struct notifier_block *this,
					unsigned long event, void *ptr)
{
	msm_rtb_ptr->enabled = enabled = 0;
	return NOTIFY_DONE;
}

static struct notifier_block msm_rtb_panic_blk = {
	.notifier_call  = msm_rtb_panic_notifier,
	.priority = INT_MAX,
};

static int notrace msm_rtb_event_should_log(enum logk_event_type log_type)
{
	return msm_rtb_ptr->initialized && enabled &&
		((1 << (log_type & ~LOGTYPE_NOPC)) & filter);
}

static inline void msm_rtb_emit_sentinel(struct msm_rtb_layout *start)
{
	start->sentinel[0] = SENTINEL_BYTE_1;
	start->sentinel[1] = SENTINEL_BYTE_2;
	start->sentinel[2] = SENTINEL_BYTE_3;
}

static inline void msm_rtb_write_type(enum logk_event_type log_type,
			struct msm_rtb_layout *start)
{
	start->log_type = (char)log_type;
}

static inline void msm_rtb_write_caller(uint64_t caller,
				struct msm_rtb_layout *start)
{
	start->caller = caller;
}

static inline void msm_rtb_write_idx(uint32_t idx,
				struct msm_rtb_layout *start)
{
	start->idx = idx;
}

static inline void msm_rtb_write_data(uint64_t data,
				struct msm_rtb_layout *start)
{
	start->data = data;
}

static inline void msm_rtb_write_timestamp(struct msm_rtb_layout *start)
{
	start->timestamp = sched_clock();
}

static inline void msm_rtb_write_cyclecount(struct msm_rtb_layout *start)
{
	start->cycle_count = get_cycles();
}

static void uncached_logk_pc_idx(enum logk_event_type log_type, uint64_t caller,
				 uint64_t data, int idx)
{
	struct msm_rtb_layout *start;

	start = msm_rtb_ptr->rtb + (idx & (msm_rtb_ptr->nentries - 1));
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

static int msm_rtb_get_idx(void)
{
	int cpu, i, offset;

	cpu = raw_smp_processor_id();
	i = atomic_add_return(msm_rtb_ptr->step_size, &msm_rtb_ptr->msm_rtb_idx[cpu].idx);
	i -= msm_rtb_ptr->step_size;

	/* Check for wrapped around */
	offset = (i & (msm_rtb_ptr->nentries - 1)) -
		 ((i - msm_rtb_ptr->step_size) & (msm_rtb_ptr->nentries - 1));
	if (offset < 0) {
		uncached_logk_timestamp(i);
		i = atomic_add_return(msm_rtb_ptr->step_size, &msm_rtb_ptr->msm_rtb_idx[cpu].idx);
		i -= msm_rtb_ptr->step_size;
	}

	return i;
}

static noinline void trace_rwmmio_write_cb(void *unused,
	unsigned long fn, u64 val, u8 width, volatile void __iomem *addr)
{
	int i;
	uint64_t caller, data;

	if (!msm_rtb_event_should_log(LOGK_WRITEL))
		return;

	i = msm_rtb_get_idx();
	caller = (uint64_t)fn;
	data = (uint64_t)addr;
	uncached_logk_pc_idx(LOGK_WRITEL, caller, data, i);
	LOG_BARRIER;
}

static noinline void trace_rwmmio_read_cb(void *unused,
	unsigned long fn, u8 width, const volatile void  __iomem *addr)
{
	int i;
	uint64_t caller, data;

	if (!msm_rtb_event_should_log(LOGK_READL))
		return;

	i = msm_rtb_get_idx();
	caller = (uint64_t)fn;
	data = (uint64_t)addr;
	uncached_logk_pc_idx(LOGK_READL, caller, data, i);
	LOG_BARRIER;
}

static noinline void trace_irq_handler_entry_cb(void *unused, int irqnr,
		struct irqaction *action)
{
	int i;
	uint64_t caller, data;

	if (!msm_rtb_event_should_log(LOGK_IRQ))
		return;

	i = msm_rtb_get_idx();
	caller = (uint64_t)action->handler;
	data = irqnr;
	uncached_logk_pc_idx(LOGK_IRQ, caller, data, i);
	LOG_BARRIER;
}

static noinline void trace_pid_cb(void *unused, bool preempt,
	struct task_struct *prev, struct task_struct *next)
{
	int i;
	uint64_t caller, data;

	if (!msm_rtb_event_should_log(LOGK_CTXID))
		return;

	i = msm_rtb_get_idx();
	caller = (uint64_t)__builtin_return_address(0);
	data = (uint64_t)next->pid;
	uncached_logk_pc_idx(LOGK_CTXID, caller, data, i);
	LOG_BARRIER;
}

static int msm_rtb_probe(struct platform_device *pdev)
{
	struct md_region md_entry;
	u64 size;
	dma_addr_t phys_addr;
	void *vaddr;
#if defined(CONFIG_QCOM_RTB_SEPARATE_CPUS)
	unsigned int cpu;
#endif
	int ret;

	if (pdev->dev.of_node) {
		ret = of_property_read_u32(pdev->dev.of_node,
				"qcom,rtb-size", (u32 *)&size);
		if (ret < 0)
			return ret;
	} else
		return -EINVAL;

	if (size <= 0 || size > SZ_1M)
		return -EINVAL;

	vaddr = dmam_alloc_coherent(&pdev->dev, size + sizeof(*msm_rtb_ptr),
					&phys_addr, GFP_KERNEL);
	if (!vaddr)
		return -ENOMEM;

	msm_rtb_ptr = vaddr;
	memset(msm_rtb_ptr, 0, size + sizeof(*msm_rtb_ptr));
	msm_rtb_ptr->rtb = vaddr + sizeof(*msm_rtb_ptr);
	msm_rtb_ptr->size = size;
	msm_rtb_ptr->phys = phys_addr + sizeof(*msm_rtb_ptr);
	msm_rtb_ptr->nentries = msm_rtb_ptr->size / sizeof(struct msm_rtb_layout);
	/* Round this down to a power of 2 */
	msm_rtb_ptr->nentries = __rounddown_pow_of_two(msm_rtb_ptr->nentries);

	strlcpy(md_entry.name, "KRTB_BUF", sizeof(md_entry.name));
	md_entry.virt_addr = (uintptr_t)msm_rtb_ptr;
	md_entry.phys_addr = phys_addr;
	md_entry.size = msm_rtb_ptr->size + sizeof(*msm_rtb_ptr);
	if (msm_minidump_add_region(&md_entry) < 0)
		pr_info("Failed to add RTB_BUF in Minidump\n");

#if defined(CONFIG_QCOM_RTB_SEPARATE_CPUS)
	for_each_possible_cpu(cpu)
		atomic_set(&msm_rtb_ptr->msm_rtb_idx[cpu].idx, cpu);

	msm_rtb_ptr->step_size = num_possible_cpus();
#else
	atomic_set(&msm_rtb_ptr->msm_rtb_idx[0].idx, 0);
	msm_rtb_ptr->step_size = 1;
#endif

	msm_rtb_ptr->enabled = enabled;
	msm_rtb_ptr->filter = filter;
	ret = register_trace_irq_handler_entry(trace_irq_handler_entry_cb, NULL);
	if (ret) {
		dev_err(&pdev->dev, "irq_handler_entry_cb registration failed\n");
		return -EINVAL;
	}

	ret = register_trace_prio_sched_switch(trace_pid_cb, NULL, 1);
	if (ret) {
		dev_err(&pdev->dev, "trace_pid_cb registration failed\n");
		unregister_trace_irq_handler_entry(trace_irq_handler_entry_cb, NULL);
		return -EINVAL;
	}

	ret = register_trace_rwmmio_write(trace_rwmmio_write_cb, NULL);
	if (ret) {
		dev_err(&pdev->dev, "trace_raw_write_cb registration failed\n");
		unregister_trace_irq_handler_entry(trace_irq_handler_entry_cb, NULL);
		unregister_trace_sched_switch(trace_pid_cb, NULL);
		return -EINVAL;
	}

	ret = register_trace_rwmmio_read(trace_rwmmio_read_cb, NULL);
	if (ret) {
		dev_err(&pdev->dev, "trace_raw_read_cb registration failed\n");
		unregister_trace_irq_handler_entry(trace_irq_handler_entry_cb, NULL);
		unregister_trace_sched_switch(trace_pid_cb, NULL);
		unregister_trace_rwmmio_write(trace_rwmmio_write_cb, NULL);
		return -EINVAL;
	}

	atomic_notifier_chain_register(&panic_notifier_list,
						&msm_rtb_panic_blk);
	msm_rtb_ptr->initialized = 1;
	return 0;
}

static int msm_rtb_remove(struct platform_device *pdev)
{
	msm_rtb_ptr->initialized = 0;
	atomic_notifier_chain_unregister(&panic_notifier_list,
						&msm_rtb_panic_blk);

	unregister_trace_rwmmio_read(trace_rwmmio_read_cb, NULL);
	unregister_trace_rwmmio_write(trace_rwmmio_write_cb, NULL);
	unregister_trace_sched_switch(trace_pid_cb, NULL);
	unregister_trace_irq_handler_entry(trace_irq_handler_entry_cb, NULL);
	return 0;
}

static const struct of_device_id msm_match_table[] = {
	{.compatible = RTB_COMPAT_STR},
	{},
};

static struct platform_driver msm_rtb_driver = {
	.probe = msm_rtb_probe,
	.remove = msm_rtb_remove,
	.driver         = {
		.name = "msm_rtb",
		.of_match_table = msm_match_table
	},
};
module_platform_driver(msm_rtb_driver);
MODULE_DESCRIPTION("Register Trace Buffer(RTB) driver");
MODULE_LICENSE("GPL v2");
