// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2018, 2019, The Linux Foundation. All rights reserved.
 */

#include <linux/of.h>
#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/perf_event.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/ktime.h>

struct llcc_pmu {
	struct pmu pmu;
	struct hlist_node node;
	void __iomem *lagg_base;
	struct perf_event event;
};

#define MON_CFG(m) ((m)->lagg_base + 0x200)
#define MON_CNT(m, cpu) ((m)->lagg_base + 0x220 + 0x4 * cpu)
#define to_llcc_pmu(ptr) (container_of(ptr, struct llcc_pmu, pmu))

#define LLCC_RD_EV 0x1000
#define ENABLE 0x01
#define CLEAR 0x10
#define DISABLE 0x00
#define SCALING_FACTOR 0x3
#define NUM_COUNTERS NR_CPUS
#define VALUE_MASK 0xFFFFFF

static u64 llcc_stats[NUM_COUNTERS];
static unsigned int users;
static raw_spinlock_t counter_lock;
static raw_spinlock_t users_lock;
static ktime_t last_read;

static int qcom_llcc_event_init(struct perf_event *event)
{
	u64 config = event->attr.config;
	u64 type = event->attr.type;

	if (config == LLCC_RD_EV) {
		event->hw.config_base = event->attr.config;
		event->readable_on_cpus = CPU_MASK_ALL;
		return 0;
	} else
		return -ENOENT;
}

static void qcom_llcc_event_read(struct perf_event *event)
{
	int i = 0, cpu = event->cpu;
	unsigned long raw, irq_flags;
	struct llcc_pmu *llccpmu = to_llcc_pmu(event->pmu);
	ktime_t cur;

	raw_spin_lock_irqsave(&counter_lock, irq_flags);
	cur = ktime_get();
	if (ktime_ms_delta(cur, last_read) > 1) {
		writel_relaxed(DISABLE, MON_CFG(llccpmu));
		for (i = 0; i < NUM_COUNTERS; i++) {
			raw = readl_relaxed(MON_CNT(llccpmu, i));
			raw &= VALUE_MASK;
			llcc_stats[i] += (u64) raw << SCALING_FACTOR;
		}
		last_read = cur;
		writel_relaxed(CLEAR, MON_CFG(llccpmu));
		writel_relaxed(ENABLE, MON_CFG(llccpmu));
	}

	if (!(event->hw.state & PERF_HES_STOPPED))
		local64_set(&event->count, llcc_stats[cpu]);
	raw_spin_unlock_irqrestore(&counter_lock, irq_flags);
}

static void qcom_llcc_event_start(struct perf_event *event, int flags)
{
	struct llcc_pmu *llccpmu = to_llcc_pmu(event->pmu);

	if (flags & PERF_EF_RELOAD)
		WARN_ON(!(event->hw.state & PERF_HES_UPTODATE));
	event->hw.state = 0;
}

static void qcom_llcc_event_stop(struct perf_event *event, int flags)
{
	struct llcc_pmu *llccpmu = to_llcc_pmu(event->pmu);

	qcom_llcc_event_read(event);
	event->hw.state |= PERF_HES_STOPPED | PERF_HES_UPTODATE;
}

static int qcom_llcc_event_add(struct perf_event *event, int flags)
{
	int i;
	unsigned int cpu = event->cpu;
	unsigned long irq_flags;
	struct llcc_pmu *llccpmu = to_llcc_pmu(event->pmu);

	raw_spin_lock(&users_lock);
	if (!users)
		writel_relaxed(ENABLE, MON_CFG(llccpmu));
	users++;
	raw_spin_unlock(&users_lock);

	event->hw.state = PERF_HES_STOPPED | PERF_HES_UPTODATE;

	if (flags & PERF_EF_START)
		qcom_llcc_event_start(event, PERF_EF_RELOAD);

	return 0;
}

static void qcom_llcc_event_del(struct perf_event *event, int flags)
{
	int i;
	unsigned int cpu = event->cpu;
	unsigned long irq_flags;
	struct llcc_pmu *llccpmu = to_llcc_pmu(event->pmu);

	raw_spin_lock(&users_lock);
	users--;
	if (!users)
		writel_relaxed(DISABLE, MON_CFG(llccpmu));
	raw_spin_unlock(&users_lock);
}

static int qcom_llcc_pmu_probe(struct platform_device *pdev)
{
	struct llcc_pmu *llccpmu;
	struct resource *res;
	int ret, i;

	llccpmu = devm_kzalloc(&pdev->dev, sizeof(struct llcc_pmu), GFP_KERNEL);
	if (!llccpmu)
		return -ENOMEM;

	llccpmu->pmu = (struct pmu) {
		.task_ctx_nr = perf_invalid_context,

		.event_init	= qcom_llcc_event_init,
		.add		= qcom_llcc_event_add,
		.del		= qcom_llcc_event_del,
		.start		= qcom_llcc_event_start,
		.stop		= qcom_llcc_event_stop,
		.read		= qcom_llcc_event_read,
		.events_across_hotplug	= 1,
	};

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "lagg-base");
	llccpmu->lagg_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(llccpmu->lagg_base)) {
		dev_err(&pdev->dev, "Can't map PMU lagg base: @%pa\n",
			&res->start);
		return PTR_ERR(llccpmu->lagg_base);
	}

	raw_spin_lock_init(&counter_lock);
	raw_spin_lock_init(&users_lock);

	ret = perf_pmu_register(&llccpmu->pmu, "llcc-pmu", -1);
	if (ret < 0)
		dev_err(&pdev->dev, "Failed to register LLCC PMU (%d)\n", ret);

	dev_info(&pdev->dev, "Registered llcc_pmu, type: %d\n",
		 llccpmu->pmu.type);

	return 0;
}

static const struct of_device_id qcom_llcc_pmu_match_table[] = {
	{ .compatible = "qcom,qcom-llcc-pmu" },
	{}
};

static struct platform_driver qcom_llcc_pmu_driver = {
	.driver = {
		.name = "qcom-llcc-pmu",
		.of_match_table = qcom_llcc_pmu_match_table,
	},
	.probe = qcom_llcc_pmu_probe,
};

module_platform_driver(qcom_llcc_pmu_driver);
