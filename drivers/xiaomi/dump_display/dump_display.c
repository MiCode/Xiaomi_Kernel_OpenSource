// SPDX-License-Identifier: GPL-2.0-only
/*
 * Dump display support
 *
 * Copyright (C) 2023.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/io.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/stacktrace.h>
#include <linux/sizes.h>
#include <linux/panic_notifier.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <asm/stacktrace.h>

#define CRASH_RECORD_MAGIC 0x12345678
#define TRACE_INFO_SIZE 2044

struct crash_info {
	unsigned int magic;
	char back_trace[TRACE_INFO_SIZE];
};

struct crash_info __iomem *crash_info_region_base;

int trace_snprint(char *buf, size_t size, const unsigned long *entries,
			unsigned int nr_entries)
{
	unsigned int generated, i, total = 0;

	if (!entries)
		return 0;

	for (i = 0; i < nr_entries && size; i++) {
		generated = snprintf(buf, size, "\n\r%pS",
				     (void *)entries[i]);

		total += generated;
		if (generated >= size) {
			buf += size;
			size = 0;
		} else {
			buf += generated;
			size -= generated;
		}
	}
	snprintf(buf , size, "\n\rcomm:%s", current->comm);
	return total;
}

static int  set_backtrace_msg(struct notifier_block *self, unsigned long v, void *p)
{
	unsigned int nr_entries;
	unsigned long entries[30] = {0};
	char *trace_record;

	nr_entries = stack_trace_save(entries,  ARRAY_SIZE(entries), 2);
	if (!nr_entries)
		return 0;

	trace_record = kmalloc(TRACE_INFO_SIZE, GFP_ATOMIC);
	if (!trace_record)
		return 0;

	trace_snprint(trace_record, TRACE_INFO_SIZE, entries, nr_entries);
	memcpy_toio(crash_info_region_base->back_trace, trace_record, TRACE_INFO_SIZE);

	kfree(trace_record);
	return 0;
}

static struct notifier_block  panic_block_to_dump = {
	.notifier_call = set_backtrace_msg
};

static int dump_display_probe(struct platform_device *pdev)
{
	char *unknow_info = "UNKNOWN";
	int  unknow_info_size  = sizeof(unknow_info);
	struct resource *res;
	u64 mem_address, mem_size;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err("failed to get dump_display address\n");
		return -EINVAL;
	}
	mem_address = res -> start;
	mem_size = resource_size(res);

	crash_info_region_base = ioremap(mem_address, mem_size);
	if (!crash_info_region_base) {
		pr_err("error to ioremap crash_record base\n");
		return -EIO;
	}

	memset_io(crash_info_region_base, 0, mem_size);
	crash_info_region_base->magic = CRASH_RECORD_MAGIC;
	memcpy_toio(crash_info_region_base->back_trace, unknow_info, unknow_info_size );

	atomic_notifier_chain_register(&panic_notifier_list,
				&panic_block_to_dump);
	return 0;
}

static const struct of_device_id ddp_match[] = {
	{ .compatible = "xiaomi,dump_display" },
	{}
};

static struct platform_driver dump_display_driver = {
	.probe		= dump_display_probe,
	.driver		= {
		.name = "dump_display",
		.of_match_table	= ddp_match,
	},
};

static int __init crash_record_init(void)
{
	pr_info("%s init\n", __func__);
	platform_driver_register(&dump_display_driver);

	return 0;
}

module_init(crash_record_init);
MODULE_LICENSE("GPL v2");
