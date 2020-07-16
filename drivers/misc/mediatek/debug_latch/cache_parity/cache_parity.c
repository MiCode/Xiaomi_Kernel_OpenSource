/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Mars.Cheng <mars.cheng@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/printk.h>
#include <linux/bug.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/irqreturn.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/sched/clock.h>
#include <mt-plat/aee.h>
#include <cache_parity.h>
#include <asm/cputype.h>
#include <linux/irqchip/mtk-gic-extend.h>

/*
 * internal weak function, by-chip implemented.
 */
void __attribute__((weak)) ecc_dump_debug_info(void)
{
	pr_notice("%s is not implemented\n", __func__);
}

#define ECC_LOG(fmt, ...) \
	do { \
		pr_notice(fmt, __VA_ARGS__); \
		aee_sram_printk(fmt, __VA_ARGS__); \
	} while (0)

static DEFINE_SPINLOCK(parity_isr_lock);

void __iomem *parity_debug_base;
static unsigned int err_level;
static unsigned int irq_count;
static unsigned int version;
static struct parity_irq_record_t *parity_irq_record;
static bool cache_error_happened;
static unsigned int cache_error_times;
static u64 cache_error_timestamp;

static irqreturn_t (*custom_parity_isr)(int irq, void *dev_id);
static int cache_parity_probe(struct platform_device *pdev);

static const struct of_device_id cache_parity_of_ids[] = {
	{   .compatible = "mediatek,cache_parity", },
	{}
};

static struct platform_driver cache_parity_drv = {
	.driver = {
		.name = "cache_parity",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = cache_parity_of_ids,
	},
	.probe = cache_parity_probe,
};

static struct cache_parity_work_data {
	struct work_struct work;
	u32 version;
	union _data {
		struct _v1 {
			u32 irq_index;
			u32 status;
		} v1;
		struct _v2 {
			u32 irq_index;
			u64 misc0_el1;
			u64 status_el1;
		} v2;
	} data;
} cache_parity_wd;

static ssize_t cache_status_show(struct device_driver *driver,
				 char *buf)
{

	if (cache_error_happened)
		return snprintf(buf, PAGE_SIZE, "True, %u times (%llu ns)\n",
				cache_error_times, cache_error_timestamp);
	else
		return snprintf(buf, PAGE_SIZE, "False\n");
}

static DRIVER_ATTR_RO(cache_status);

#ifdef CONFIG_ARM64
static u64 read_ERXMISC0_EL1(void)
{
	u64 v;

	__asm__ volatile ("mrs %0, s3_0_c5_c5_0" : "=r" (v));

	return v;
}

static u64 read_ERXSTATUS_EL1(void)
{
	u64 v;

	__asm__ volatile ("mrs %0, s3_0_c5_c4_2" : "=r" (v));

	return v;
}

static void write_ERXSTATUS_EL1(u64 v)
{
	__asm__ volatile ("msr s3_0_c5_c4_2, %0" : : "r" (v));
}

static void write_ERXSELR_EL1(u32 v)
{
	__asm__ volatile ("msr s3_0_c5_c3_1, %0" : : "r" (v));
}
#else
/* TODO: aarch32, TBD */
static u64 read_ERXMISC0_EL1(void)
{
	return 0;
}

static u64 read_ERXSTATUS_EL1(void)
{
	return 0;
}

static void write_ERXSTATUS_EL1(u64 v)
{
}

static void write_ERXSELR_EL1(u32 v)
{
}
#endif

static void handle_error(struct work_struct *w)
{
	struct cache_parity_work_data *wd;

	wd = container_of(w, struct cache_parity_work_data, work);

	if (wd->version == 1) {
		aee_kernel_exception("cache parity",
			"cache parity error,%s:%d,%s:0x%x\n\n%s\n",
			"irq_index", wd->data.v1.irq_index,
			"status", wd->data.v1.status,
			"CRDISPATCH_KEY:Cache Parity Issue");
	} else if (wd->version == 2) {
		char error_type[ERROR_TYPE_BUF_LENGTH];
		int ret;

		if (cache_parity_wd.data.v2.status_el1 & ECC_UE_BIT)
			ret = snprintf(error_type, ERROR_TYPE_BUF_LENGTH, "UE");
		else if (cache_parity_wd.data.v2.status_el1 & ECC_CE_BIT)
			ret = snprintf(error_type, ERROR_TYPE_BUF_LENGTH, "CE");
		else if (cache_parity_wd.data.v2.status_el1 & ECC_DE_BIT)
			ret = snprintf(error_type, ERROR_TYPE_BUF_LENGTH, "DE");
		else
			ret = snprintf(error_type, ERROR_TYPE_BUF_LENGTH, "NA");

		aee_kernel_exception("cache parity",
			"ecc error(%s), %s:%d, %s:%016llx, %s:%016llx\n\n%s\n",
			error_type, "irq_index", wd->data.v2.irq_index,
			"misc0_el1", cache_parity_wd.data.v2.misc0_el1,
			"status_el1", cache_parity_wd.data.v2.status_el1,
			"CRDISPATCH_KEY:Cache Parity Issue");
	} else {
		pr_debug("Unknown Cache Error Irq\n");
	}
}

static irqreturn_t default_parity_isr_v2(int irq, void *dev_id)
{
#ifdef CONFIG_ARM64_ERRATUM_1800710
	static const struct midr_range erratum_1800710_cpu_list[] = {
		_MIDR_ALL_VERSIONS(MIDR_CORTEX_A76),
		_MIDR_ALL_VERSIONS(MIDR_CORTEX_A77),
	};
#endif

	u32 hwirq = virq_to_hwirq(irq);

	ecc_dump_debug_info();

	cache_error_happened = true;
	cache_error_times++;

	write_ERXSELR_EL1(((hwirq - FAULTIRQ_START) == 0) ? 1 : 0);

	/* collect error status to report later */
	cache_parity_wd.data.v2.irq_index = hwirq;
	cache_parity_wd.data.v2.misc0_el1 = read_ERXMISC0_EL1();
	cache_parity_wd.data.v2.status_el1 = read_ERXSTATUS_EL1();

	/* clear error status to make irq not pending */
	write_ERXSTATUS_EL1(cache_parity_wd.data.v2.status_el1);

	/*
	 * if we read 0x0 from status, force clear all error
	 * to de-assert nFAULTIRQ
	 */
	if (!cache_parity_wd.data.v2.misc0_el1 &&
			!cache_parity_wd.data.v2.status_el1) {
		write_ERXSTATUS_EL1(0xFFC00000);
	}

	/* to ensure all transactions are finished */
	dsb(sy);

	/* OK, can start a worker to generate error report */
	schedule_work(&cache_parity_wd.work);

	/* get current kernel time in nanosecond */
	cache_error_timestamp = local_clock();

	ECC_LOG("ecc error,%s:%d, %s: 0x%016llx, %s: 0x%016llx\n",
	       "irq_index", cache_parity_wd.data.v2.irq_index,
	       "misc0_el1", cache_parity_wd.data.v2.misc0_el1,
	       "status_el1", cache_parity_wd.data.v2.status_el1);

#ifdef CONFIG_ARM64_ERRATUM_1800710
	if (is_midr_in_range_list(read_cpuid_id(), erratum_1800710_cpu_list)) {
		if ((cache_parity_wd.data.v2.status_el1 & ECC_CE_BIT) == 0x2 &&
		 (cache_parity_wd.data.v2.status_el1 & ECC_SERR_BIT) == 0x2) {
			ECC_LOG("%s %s hit, may cause stale translation\n",
					__func__, "Erratum 1800710");
		}
	}
#endif

	if (cache_error_times > ECC_IRQ_TRIGGER_THRESHOLD) {
		disable_irq_nosync(irq);
		ECC_LOG("%s disable IRQ%d due to trigger over than %d times.",
			__func__,
			cache_parity_wd.data.v2.irq_index,
			ECC_IRQ_TRIGGER_THRESHOLD);
	}

	return IRQ_HANDLED;
}

static irqreturn_t default_parity_isr_v1(int irq, void *dev_id)
{
	struct parity_record_t *parity_record;
	unsigned int status;
	unsigned int offset;
	unsigned int irq_idx;
	unsigned int i;

	cache_error_happened = true;

	for (i = 0, parity_record = NULL; i < irq_count; i++) {
		if (parity_irq_record[i].irq == irq) {
			irq_idx = i;
			parity_record = &(parity_irq_record[i].parity_record);
			pr_info("parity isr for %d\n", i);
			break;
		}
	}

	if (parity_record == NULL) {
		pr_info("no matched irq %d\n", irq);
		return IRQ_HANDLED;
	}

	status = readl(parity_debug_base + parity_record->check_offset);
	pr_info("status 0x%x\n", status);

	if (status & parity_record->check_mask)
		pr_info("detect cache parity error\n");
	else
		pr_info("no cache parity error\n");

	for (i = 0; i < parity_record->dump_length; i += 4) {
		offset = parity_record->dump_offset + i;
		pr_info("offset 0x%x, val 0x%x\n", offset,
			readl(parity_debug_base + offset));
	}

#ifdef CONFIG_MTK_ENG_BUILD
	WARN_ON(1);
#else
	if (err_level) {
		cache_parity_wd.data.v1.irq_index = irq_idx;
		cache_parity_wd.data.v1.status = status;
		schedule_work(&cache_parity_wd.work);
	} else
		WARN_ON(1);
#endif

	spin_lock(&parity_isr_lock);

	if (parity_record->clear_mask) {
		writel(parity_record->clear_mask,
			parity_debug_base + parity_record->clear_offset);
		dsb(sy);
		writel(0x0,
			parity_debug_base + parity_record->clear_offset);
		dsb(sy);

		while (readl(parity_debug_base + parity_record->check_offset) &
			parity_record->check_mask) {
			udelay(1);
		}
	}

	spin_unlock(&parity_isr_lock);

	return IRQ_HANDLED;
}

void __attribute__((weak)) cache_parity_init_platform(void)
{
	pr_info("[%s] adopt default flow\n", __func__);
}

static int cache_parity_probe_v2(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	unsigned int i;
	int ret;
	int irq;

	cache_parity_init_platform();

	ret = of_property_read_u32(node, "err_level", &err_level);
	if (ret)
		return ret;

	irq_count = of_irq_count(node);

	for (i = 0; i < irq_count; i++) {
		irq = irq_of_parse_and_map(node, i);
		pr_debug("irq %d for cpu%d\n", irq, i);

		/*
		 * we only need to bind nFAULTIRQ[n:1] to the
		 * corressponding cpu, and the end of list is
		 * dsu, which is able to serve by any core.
		 */
		if (i < irq_count - 1) {
			ret = irq_force_affinity(irq, cpumask_of(i));
			if (ret)
				pr_notice("irq target to cpu(%d) fail\n", i);
		}

		if (custom_parity_isr)
			ret = request_irq(irq, custom_parity_isr,
				IRQF_TRIGGER_NONE | IRQF_ONESHOT,
				"cache_parity", &cache_parity_drv);
		else
			ret = request_irq(irq, default_parity_isr_v2,
				IRQF_TRIGGER_NONE | IRQF_ONESHOT,
				"cache_parity", &cache_parity_drv);
		if (ret != 0)
			pr_notice("request_irq(%d) fail\n", i);
	}

	return 0;
}

static int cache_parity_probe_v1(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct parity_irq_config_t *parity_irq_config;
	size_t size;
	unsigned int i;
	unsigned int target_cpu;
	int ret;
	int irq;

	cache_parity_init_platform();

	ret = of_property_read_u32(node, "err_level", &err_level);
	if (ret)
		return ret;

	irq_count = of_irq_count(node);
	pr_info("irq_count: %d, err_level: %d\n", irq_count, err_level);

	size = sizeof(struct parity_irq_record_t) * irq_count;
	parity_irq_record = kmalloc(size, GFP_KERNEL);
	if (!parity_irq_record)
		return -ENOMEM;

	size = sizeof(struct parity_irq_config_t) * irq_count;
	parity_irq_config = kmalloc(size, GFP_KERNEL);
	if (!parity_irq_config)
		return -ENOMEM;

	size = size >> 2;
	of_property_read_variable_u32_array(node, "irq_config",
		(u32 *)parity_irq_config, size, size);

	for (i = 0; i < irq_count; i++) {
		memcpy(
			&(parity_irq_record[i].parity_record),
			&(parity_irq_config[i].parity_record),
			sizeof(struct parity_record_t));

		irq = irq_of_parse_and_map(node, i);
		parity_irq_record[i].irq = irq;
		pr_info("get %d for %d\n", irq, i);

		target_cpu = parity_irq_config[i].target_cpu;
		if (target_cpu != 1024) {
			ret = irq_set_affinity(irq, cpumask_of(target_cpu));
			if (ret)
				pr_info("target_cpu(%d) fail\n", i);
		}

		if (custom_parity_isr)
			ret = request_irq(irq, custom_parity_isr,
				IRQF_TRIGGER_NONE, "cache_parity",
				&cache_parity_drv);
		else
			ret = request_irq(irq, default_parity_isr_v1,
				IRQF_TRIGGER_NONE, "cache_parity",
				&cache_parity_drv);
		if (ret != 0)
			pr_info("request_irq(%d) fail\n", i);
	}

	kfree(parity_irq_config);

	return 0;
}

static int cache_parity_probe(struct platform_device *pdev)
{
	int ret;

	ret = of_property_read_u32(pdev->dev.of_node, "version", &version);
	if (ret)
		return ret;

	INIT_WORK(&cache_parity_wd.work, handle_error);
	cache_parity_wd.version = version;

	switch (version) {
	case 1:
		parity_debug_base = of_iomap(pdev->dev.of_node, 0);
		if (!parity_debug_base)
			return -ENOMEM;

		return cache_parity_probe_v1(pdev);
	case 2:
		return cache_parity_probe_v2(pdev);
	default:
		pr_info("unsupported version\n");
		return 0;
	}
}

static int __init cache_parity_init(void)
{
	int ret;

	ret = platform_driver_register(&cache_parity_drv);
	if (ret)
		return ret;

	ret = driver_create_file(&cache_parity_drv.driver,
				 &driver_attr_cache_status);
	if (ret)
		return ret;

	return 0;
}

module_init(cache_parity_init);
