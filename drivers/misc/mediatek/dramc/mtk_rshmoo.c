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
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/vmalloc.h>
#include <linux/memblock.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <asm/cacheflush.h>
/* #include <mach/mtk_clkmgr.h> */
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <asm/setup.h>
#include <mt-plat/mtk_io.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/mtk_meminfo.h>
#include <mt-plat/mtk_chip.h>
#include <mt-plat/aee.h>
#ifdef CONFIG_MTK_WATCHDOG
#include <mtk_wd_api.h>
#endif

#include "mtk_dramc.h"
#include "dramc.h"

#ifdef RUNTIME_SHMOO
static unsigned int rshmoo_test_rank;
static unsigned int rshmoo_test_ch;
static unsigned int rshmoo_test_byte;
static unsigned int rshmoo_test_txvrange;
static unsigned int rshmoo_test_txvref;
static unsigned int rshmoo_test_delay;
static unsigned int rshmoo_test_ongoing;
static unsigned int rshmoo_test_done;
static struct timer_list rshmoo_timer;
static unsigned int rshmoo_timer_counter;

#define KEY_RSHMOO_STORE 0x9487
#define RSHMOO_STORE_MAGIC 0x04879487

#define RSHMOO_REBOOT_TIME	30
#define TIMER_INTERVAL 500
#define TIMER_EXPIRE_MINUTES(m)        ((m)*60*1000/TIMER_INTERVAL)

struct rshmoo_store_info {
	unsigned int magic;
	unsigned int last_result;
	unsigned int pass_cnt;
};

void __iomem *get_dbg_info_base(unsigned int key);

static ssize_t rshmoo_test_done_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", rshmoo_test_done);
}

DRIVER_ATTR(rshmoo_test_done, 0444,
rshmoo_test_done_show, NULL);

static ssize_t rshmoo_test_info_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
			"CH %c,RANK %u,BYTE %u,VRANGE %u,VREF %u,PI %u\n",
			(rshmoo_test_ch == 0) ? 'A' : 'B', rshmoo_test_rank,
			rshmoo_test_byte, rshmoo_test_txvrange,
			rshmoo_test_txvref, rshmoo_test_delay);
}

DRIVER_ATTR(rshmoo_test_info, 0444,
rshmoo_test_info_show, NULL);

int dram_rshmoo_mark_pass(void)
{
	struct rshmoo_store_info *ptr = (struct rshmoo_store_info *)
		get_dbg_info_base(KEY_RSHMOO_STORE);

	if (ptr && ptr->magic == RSHMOO_STORE_MAGIC) {
		ptr->last_result = 1;
		return 0;
	}
	return -1;
}

void dram_rshmoo_timer_callback(unsigned long data)
{
	unsigned int expire = TIMER_EXPIRE_MINUTES(RSHMOO_REBOOT_TIME);
#ifdef CONFIG_MTK_WATCHDOG
	int res;
	struct wd_api *wd_api = NULL;
#endif

	rshmoo_timer_counter++;

	if (rshmoo_timer_counter < expire) {
		mod_timer(&rshmoo_timer, jiffies +
				msecs_to_jiffies(TIMER_INTERVAL));
		return;
	}

#ifdef CONFIG_MTK_WATCHDOG
	res = get_wd_api(&wd_api);
	if (res < 0) {
		pr_info("arch_reset, get wd api error %d\n", res);
		while (1)
			cpu_relax();
	} else {
		pr_info("exception reboot\n");
		wd_api->wd_sw_reset(WD_SW_RESET_BYPASS_PWR_KEY);
	}
#else
	emergency_restart();
#endif
}

static int dram_rshmoo_probe(struct platform_device *pdev)
{
	int ret = 0;

	pr_debug("[DRAMC] rshmoo module probe.\n");

	ret = of_property_read_u32(pdev->dev.of_node, "rank",
			&rshmoo_test_rank);
	if (ret < 0) {
		pr_info("Fail to find rank information\n");
		return ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "ch", &rshmoo_test_ch);
	if (ret < 0) {
		pr_info("Fail to find ch information\n");
		return ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "byte",
			&rshmoo_test_byte);
	if (ret < 0) {
		pr_info("Fail to find byte information\n");
		return ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "txvrange",
			&rshmoo_test_txvrange);
	if (ret < 0) {
		pr_info("Fail to find txvrange information\n");
		return ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "txvref",
			&rshmoo_test_txvref);
	if (ret < 0) {
		pr_info("Fail to find txvref information\n");
		return ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "delay",
			&rshmoo_test_delay);
	if (ret < 0) {
		pr_info("Fail to find delay information\n");
		return ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "ongoing",
			&rshmoo_test_ongoing);
	if (ret < 0) {
		pr_info("Fail to find ongoing information\n");
		return ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "done",
			&rshmoo_test_done);
	if (ret < 0) {
		pr_info("Fail to find done information\n");
		return ret;
	}

	ret = driver_create_file(pdev->dev.driver,
	&driver_attr_rshmoo_test_done);
	if (ret) {
		pr_info("fail to create the rshmoo_test_done sysfs files\n");
		return ret;
	}

	ret = driver_create_file(pdev->dev.driver,
	&driver_attr_rshmoo_test_info);
	if (ret) {
		pr_info("fail to create the rshmoo_test_info sysfs files\n");
		return ret;
	}

	init_timer(&rshmoo_timer);
	rshmoo_timer.function = dram_rshmoo_timer_callback;
	rshmoo_timer.data = 0;
	mod_timer(&rshmoo_timer, jiffies + msecs_to_jiffies(TIMER_INTERVAL));

	return 0;
}

static int dram_rshmoo_remove(struct platform_device *dev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id dram_of_ids[] = {
	{.compatible = "mediatek,dram_rshmoo_info",},
	{}
};
#endif

static struct platform_driver dram_rshmoo_drv = {
	.probe = dram_rshmoo_probe,
	.remove = dram_rshmoo_remove,
	.driver = {
		.name = "dram_rshmoo",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = dram_of_ids,
#endif
		},
};

static int __init dram_rshmoo_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&dram_rshmoo_drv);
	if (ret) {
		pr_info("[DRAMC] init fail, ret 0x%x\n", ret);
		return ret;
	}

	return 0;
}

late_initcall(dram_rshmoo_init);
#endif
