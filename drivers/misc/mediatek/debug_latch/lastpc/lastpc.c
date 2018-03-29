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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/cpumask.h>
#include <linux/list.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <mt-plat/mt_chip.h>
#include <mt-plat/mt_lastpc.h>
#include "lastpc.h"

static const struct of_device_id lastpc_of_ids[] = {
	{ .compatible = "mediatek,lastpc-v1", },
	{ .compatible = "mediatek,mt6580-mcucfg", },
	{ .compatible = "mediatek,mt6735-mcucfg", },
	{ .compatible = "mediatek,mt8163-mcucfg", },
	{ .compatible = "mediatek,mt8167-mcucfg", },
	{ .compatible = "mediatek,mt8173-mcucfg", },
	{}
};

static int lastpc_probe(struct platform_device *pdev);
static int lastpc_remove(struct platform_device *pdev);
static int lastpc_suspend(struct platform_device *pdev, pm_message_t state);
static int lastpc_resume(struct platform_device *pdev);

static char *lastpc_dump_buf;

static struct lastpc lastpc_drv = {
	.plt_drv = {
		.driver = {
			.name = "lastpc",
			.bus = &platform_bus_type,
			.owner = THIS_MODULE,
			.of_match_table = lastpc_of_ids,
		},
		.probe = lastpc_probe,
		.remove = lastpc_remove,
		.suspend = lastpc_suspend,
		.resume = lastpc_resume,
	},
};

static int lastpc_probe(struct platform_device *pdev)
{
	struct lastpc_plt *plt = NULL;

	pr_debug("%s:%d: enter\n", __func__, __LINE__);

	plt = lastpc_drv.cur_plt;

	if (plt && plt->ops && plt->ops->probe)
		return plt->ops->probe(plt, pdev);

	lastpc_drv.base = of_iomap(pdev->dev.of_node, 0);
	if (!lastpc_drv.base)
		return -ENODEV;

	return 0;
}

static int lastpc_remove(struct platform_device *pdev)
{
	struct lastpc_plt *plt = lastpc_drv.cur_plt;

	pr_debug("%s:%d: enter\n", __func__, __LINE__);

	if (plt && plt->ops && plt->ops->remove)
		return plt->ops->remove(plt, pdev);

	return 0;
}

static int lastpc_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct lastpc_plt *plt = lastpc_drv.cur_plt;

	pr_debug("%s:%d: enter\n", __func__, __LINE__);

	if (plt && plt->ops && plt->ops->suspend)
		return plt->ops->suspend(plt, pdev, state);

	return 0;
}

static int lastpc_resume(struct platform_device *pdev)
{
	struct lastpc_plt *plt = lastpc_drv.cur_plt;

	pr_debug("%s:%d: enter\n", __func__, __LINE__);

	if (plt && plt->ops && plt->ops->resume)
		return plt->ops->resume(plt, pdev);

	return 0;
}

int lastpc_register(struct lastpc_plt *plt)
{
	if (!plt) {
		pr_warn("%s%d: plt is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	plt->common = &lastpc_drv;
	lastpc_drv.cur_plt = plt;

	return 0;
}

int lastpc_dump(char *buf, int len)
{
	struct lastpc_plt *plt = lastpc_drv.cur_plt;

	if (!buf) {
		pr_warn("%s:%d: buf is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!plt) {
		pr_warn("%s:%d: plt is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (!plt->common) {
		pr_warn("%s:%d: plt->common is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (!plt->common->base) {
		pr_warn("%s:%d: plt->common->base is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->ops && plt->ops->dump)
		return plt->ops->dump(plt, buf, len);

	pr_warn("no dump function implemented\n");
	return 0;
}

int lastpc_dump_min_len(void)
{
	struct lastpc_plt *plt = lastpc_drv.cur_plt;

	if (!plt) {
		pr_warn("%s:%d: plt is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (!plt->min_buf_len)
		pr_warn("%s:%d: min_buf_len is 0\n", __func__, __LINE__);

	return plt->min_buf_len;
}

int mt_reg_dump(char *buf)
{
	strncpy(buf, lastpc_dump_buf, strlen(lastpc_dump_buf)+1);
	return 0;
}

static ssize_t lastpc_dump_show(struct device_driver *driver, char *buf)
{
	struct lastpc_plt *plt = lastpc_drv.cur_plt;
	int ret = 0;

	ret = plt->ops->dump(plt, buf, -1);
	if (ret)
		pr_err("%s:%d: dump failed\n", __func__, __LINE__);

	return strlen(buf);
}

static ssize_t lastpc_dump_store(struct device_driver *driver, const char *buf, size_t count)
{
	return count;
}

DRIVER_ATTR(lastpc_dump, 0664, lastpc_dump_show, lastpc_dump_store);

static ssize_t lastpc_reboot_test_show(struct device_driver *driver, char *buf)
{

	return snprintf(buf, PAGE_SIZE, "==LAST PC test==\n"
					"1.LAST PC Reboot test\n");

}

static ssize_t lastpc_reboot_test_store(struct device_driver *driver, const char *buf, size_t count)
{
	struct lastpc_plt *plt = lastpc_drv.cur_plt;
	char *p = (char *)buf;
	unsigned long num = 0;

	if (kstrtoul(p, 10, &num) != 0) {
		pr_err("%s:%d: kstrtoul fail for %s\n", __func__, __LINE__, p);
		return 0;
	}

	switch (num) {
	case 1:
		if (plt->ops->reboot_test(plt) != 0)
			pr_err("%s:%d: reboot test failed\n", __func__, __LINE__);
		break;
	default:
		break;
	}

	return count;
}

DRIVER_ATTR(lastpc_reboot_test, 0664, lastpc_reboot_test_show, lastpc_reboot_test_store);

static int lastpc_start(void)
{
	struct lastpc_plt *plt = lastpc_drv.cur_plt;

	if (!plt->ops) {
		pr_err("%s:%d: ops not installed\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->ops->start)
		return plt->ops->start(plt);

	return 0;
}

static int __init lastpc_init(void)
{
	int ret = 0;

	ret = lastpc_start();
	if (ret) {
		pr_err("%s:%d: lastpc_start failed\n", __func__, __LINE__);
		return -ENODEV;
	}

	/* since kernel already populates dts, our probe would be callback after this registration */
	ret = platform_driver_register(&lastpc_drv.plt_drv);
	if (ret) {
		pr_err("%s:%d: platform_driver_register failed\n", __func__, __LINE__);
		return -ENODEV;
	}

	ret = driver_create_file(&lastpc_drv.plt_drv.driver, &driver_attr_lastpc_dump);
	if (ret)
		pr_err("%s:%d: driver_create_file failed.\n", __func__, __LINE__);

	ret = driver_create_file(&lastpc_drv.plt_drv.driver, &driver_attr_lastpc_reboot_test);
	if (ret)
		pr_err("%s:%d: driver_create_file failed.\n", __func__, __LINE__);

	lastpc_dump_buf = kzalloc(lastpc_drv.cur_plt->min_buf_len, GFP_KERNEL);
	if (!lastpc_dump_buf)
		return -ENOMEM;

	/* we dump here and then return lastpc_dump_buf to users to prevent lastpc values cleaned by low power mechanism
		(MCUSYS might be turned off before lastpc_dump()) */
	lastpc_dump(lastpc_dump_buf, lastpc_drv.cur_plt->min_buf_len);

	return 0;
}

static void __exit lastpc_exit(void)
{
}

module_init(lastpc_init);
module_exit(lastpc_exit);
