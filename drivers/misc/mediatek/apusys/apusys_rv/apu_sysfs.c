// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#include "apu.h"

static ssize_t rv33_coredump(struct file *filep,
		struct kobject *kobj, struct bin_attribute *attr,
		char *buf, loff_t offset, size_t size)
{
	unsigned int length = 0;
	struct platform_device *pdev;
	struct device *dev;
	struct mtk_apu *apu;
	unsigned int coredump_length;

	dev = container_of(kobj, struct device, kobj);
	pdev = container_of(dev, struct platform_device, dev);
	apu = platform_get_drvdata(pdev);
	coredump_length = TCM_SIZE + DRAM_DUMP_SIZE + REG_SIZE + TBUF_SIZE;

	if (offset >= 0 && offset < coredump_length) {
		if ((offset + size) > coredump_length)
			size = coredump_length - offset;

		/* XXX invalid cache here? */
		memcpy(buf, apu->coredump_buf + offset, size);
		length = size;
	}

	return length;
}

struct bin_attribute bin_attr_core_dump = {
	.attr = {
		.name = "coredump.bin",
		.mode = 0444,
	},
	.size = 0,
	.read = rv33_coredump,
};

int apu_sysfs_init(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;

	ret = sysfs_create_bin_file(&dev->kobj, &bin_attr_core_dump);
	if (ret < 0) {
		pr_info("%s: sysfs create fail for core_dump(%d)\n",
			__func__, ret);
		goto end;
	}

end:
	return ret;
}

void apu_sysfs_remove(struct platform_device *pdev)
{
	sysfs_remove_bin_file(&pdev->dev.kobj, &bin_attr_core_dump);
}
