// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/signal.h>
#include <linux/cpu.h>
#include "systracker_v2.h"

void __iomem *BUS_DBG_BASE;
void __iomem *BUS_DBG_INFRA_BASE;
struct systracker_config_t track_config;
struct systracker_entry_t track_entry;

static const struct of_device_id systracker_of_ids[] = {
	{ .compatible = "mediatek,bus_dbg-v2", },
	{}
};

static struct mt_systracker_driver mt_systracker_drv = {
	.driver = {
		.driver = {
			.name = "systracker",
			.bus = &platform_bus_type,
			.owner = THIS_MODULE,
			.of_match_table = systracker_of_ids,
		},
		.probe = systracker_probe,
		.remove = systracker_remove,
		.suspend = systracker_suspend,
		.resume = systracker_resume,
	},
	.device = {
		.name = "systracker",
	},
};

static int systracker_probe(struct platform_device *pdev)
{
	void __iomem *infra_ao_base;
	unsigned int bus_dbg_con_offset;

	pr_notice("systracker probe\n");

	/* iomap register */
	BUS_DBG_BASE = of_iomap(pdev->dev.of_node, 0);
	if (!BUS_DBG_BASE) {
		pr_notice("can't of_iomap for systracker!!\n");
		return -ENOMEM;
	}

	pr_notice("of_iomap for systracker @ 0x%p\n", BUS_DBG_BASE);

	infra_ao_base = of_iomap(pdev->dev.of_node, 1);
	if (!infra_ao_base) {
		pr_notice("[systracker] bus_dbg_con is in infra\n");
		BUS_DBG_INFRA_BASE = BUS_DBG_BASE;
	} else {
		pr_notice("[systracker] bus_dbg_con is in infra_ao\n");
		if (of_property_read_u32
			(pdev->dev.of_node, "mediatek,bus_dbg_con_offset",
			&bus_dbg_con_offset)) {
			pr_notice
			("[systracker] cannot get bus_dbg_con_offset\n");
			return -ENODEV;
		}
		BUS_DBG_INFRA_BASE = infra_ao_base + bus_dbg_con_offset;
	}

	/* save entry info */
	save_entry();
	memset(&track_config, 0, sizeof(struct systracker_config_t));

	track_config.enable_timeout = 1;
	track_config.enable_slave_err = 1;
	track_config.enable_irq = 0;
	track_config.timeout_ms = 100;
	track_config.timeout2_ms = 2000;

	systracker_reset();
	systracker_enable();

	return 0;
}

int systracker_remove(struct platform_device *pdev)
{
	return 0;
}

int systracker_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int systracker_resume_default(struct platform_device *pdev)
{
	if (track_config.state || track_config.enable_wp)
		systracker_enable();

	return 0;
}

int systracker_resume(struct platform_device *pdev)
{
	return systracker_resume_default(pdev);
}

void save_entry(void)
{
	int i = 0;

	track_entry.dbg_con =  readl(BUS_DBG_CON);
	track_entry.dbg_con_infra =  readl(BUS_DBG_CON_INFRA);

	for (i = 0; i < BUS_DBG_NUM_TRACKER; i++) {
		track_entry.ar_track_l[i]   =
			readl(BUS_DBG_AR_TRACK_L(i));
		track_entry.ar_track_h[i]   =
			readl(BUS_DBG_AR_TRACK_H(i));
		track_entry.ar_trans_tid[i] =
			readl(BUS_DBG_AR_TRANS_TID(i));
		track_entry.aw_track_l[i]   =
			readl(BUS_DBG_AW_TRACK_L(i));
		track_entry.aw_track_h[i]   =
			readl(BUS_DBG_AW_TRACK_H(i));
		track_entry.aw_trans_tid[i] =
			readl(BUS_DBG_AW_TRANS_TID(i));
	}

	track_entry.w_track_data6 = readl(BUS_DBG_W_TRACK_DATA6);
	track_entry.w_track_data7 = readl(BUS_DBG_W_TRACK_DATA7);
	track_entry.w_track_data_valid =
		readl(BUS_DBG_W_TRACK_DATA_VALID);
}

static int systracker_watchpoint_enable_default(void)
{
	unsigned int con;

	track_config.enable_wp = 1;

	writel(track_config.wp_phy_address, BUS_DBG_WP);
	writel(0x00000000, BUS_DBG_WP_MASK);

	con = readl(BUS_DBG_CON_INFRA) | BUS_DBG_CON_WP_EN;
	writel(con, BUS_DBG_CON_INFRA);
	/* ensure access complete */
	mb();

	return 0;
}

int systracker_watchpoint_enable(void)
{
	return systracker_watchpoint_enable_default();
}

static int systracker_watchpoint_disable_default(void)
{
	track_config.enable_wp = 0;
	writel(readl(BUS_DBG_CON_INFRA) & (~BUS_DBG_CON_WP_EN),
		BUS_DBG_CON_INFRA);
	/* ensure access complete */
	mb();

	return 0;
}

int systracker_watchpoint_disable(void)
{
	return systracker_watchpoint_disable_default();
}

void systracker_reset_default(void)
{
	writel(readl(BUS_DBG_CON) |
		BUS_DBG_CON_SW_RST, BUS_DBG_CON);
	writel(readl(BUS_DBG_CON) |
		BUS_DBG_CON_IRQ_CLR, BUS_DBG_CON);
	writel(readl(BUS_DBG_CON) |
		BUS_DBG_CON_TIMEOUT_CLR, BUS_DBG_CON);
	/* ensure access complete */
	mb();
}

void systracker_reset(void)
{
	systracker_reset_default();
}

unsigned int systracker_timeout_value_default(void)
{
	/* prescale = (133 * (10 ^ 6)) / 16 = 8312500/s */
	return (BUS_DBG_BUS_MHZ * 1000 / 16) * track_config.timeout_ms;
}

unsigned int systracker_timeout2_value_default(void)
{
	/* prescale = (133 * (10 ^ 6)) / 16 = 8312500/s */
	return (BUS_DBG_BUS_MHZ * 1000 / 16) * track_config.timeout2_ms;
}

unsigned int systracker_timeout_value(void)
{
	return systracker_timeout_value_default();
}

unsigned int systracker_timeout2_value(void)
{
	return systracker_timeout2_value_default();
}

void systracker_enable_default(void)
{
	unsigned int con;
	unsigned int timer_control_value;

	timer_control_value = systracker_timeout_value();
#ifndef CONFIG_FPGA_EARLY_PORTING
	writel(timer_control_value, BUS_DBG_TIMER_CON0);
#else
	writel(BUS_DBG_MAX_TIMEOUT_VAL, BUS_DBG_TIMER_CON0);
#endif

	timer_control_value = systracker_timeout2_value();
#ifndef CONFIG_FPGA_EARLY_PORTING
	writel(timer_control_value, BUS_DBG_TIMER_CON1);
#else
	writel(BUS_DBG_MAX_TIMEOUT_VAL, BUS_DBG_TIMER_CON1);
#endif

	track_config.state = 1;
	con = BUS_DBG_CON_BUS_DBG_EN | BUS_DBG_CON_BUS_OT_EN;
	if (track_config.enable_timeout)
		con |= BUS_DBG_CON_TIMEOUT_EN;

	if (track_config.enable_slave_err)
		con |= BUS_DBG_CON_SLV_ERR_EN;

	if (track_config.enable_irq) {
		con |= BUS_DBG_CON_IRQ_EN;
		con &= ~BUS_DBG_CON_IRQ_WP_EN;
	}

	con |= BUS_DBG_CON_HALT_ON_EN;
	writel(con, BUS_DBG_CON_INFRA);
	/* ensure access complete */
	mb();
}

void systracker_enable(void)
{
	systracker_enable_default();
}

void enable_systracker(void)
{
	systracker_enable();
}

static void systracker_disable_default(void)
{
	track_config.state = 0;
	writel(readl(BUS_DBG_CON_INFRA) &
		~BUS_DBG_CON_BUS_DBG_EN, BUS_DBG_CON_INFRA);
	/* ensure access complete */
	mb();
}

void systracker_disable(void)
{
	systracker_disable_default();
}

int systracker_test_init(void)
{
	if (mt_systracker_drv.systracker_test_init)
		return mt_systracker_drv.systracker_test_init();

	pr_notice("mt_systracker_drv.%s is NULL", __func__);
	return -1;
}

int tracker_dump(char *buf)
{

	char *ptr = buf;
	unsigned int reg_value;
	int i;
	unsigned int entry_valid;
	unsigned int entry_secure;
	unsigned int entry_tid;
	unsigned int entry_id;
	unsigned int entry_address;
	unsigned int entry_data_size;
	unsigned int entry_burst_length;

	{
		/* Get tracker info and save to buf */

		/* BUS_DBG_AR_TRACK_L(__n)
		 * [31:0] ARADDR: DBG read tracker entry read address
		 */

		/* BUS_DBG_AR_TRACK_H(__n)
		 * [21] Valid:DBG read tracker entry valid
		 * [20:8] ARID:DBG read tracker entry read ID
		 * [6:4] ARSIZE:DBG read tracker entry read data size
		 * [3:0] ARLEN: DBG read tracker entry read burst length
		 */

		/* BUS_DBG_AR_TRACK_TID(__n)
		 * [2:0] BUS_DBG_AR_TRANS0_ENTRY_ID:
		 * DBG read tracker entry ID of 1st transaction
		 */

		ptr += sprintf(ptr,
	"[TRACKER] BUS_DBG_CON = (0x%x, 0x%x), T0= 0x%x, T1 = 0x%x\n",
				track_entry.dbg_con,
				readl(BUS_DBG_CON),
				readl(BUS_DBG_TIMER_CON0),
				readl(BUS_DBG_TIMER_CON1));

		ptr += sprintf(ptr, "BUS_DBG_CON_INFRA = (0x%x, 0x%x)\n",
			       track_entry.dbg_con_infra,
			       readl(BUS_DBG_CON_INFRA));

		for (i = 0; i < BUS_DBG_NUM_TRACKER; i++) {
			entry_address       = track_entry.ar_track_l[i];
			reg_value           = track_entry.ar_track_h[i];
			entry_valid         =
				extract_n2mbits(reg_value, 22, 22);
			entry_secure         =
				extract_n2mbits(reg_value, 21, 21);
			entry_id            =
				extract_n2mbits(reg_value, 8, 20);
			entry_data_size     =
				extract_n2mbits(reg_value, 4, 6);
			entry_burst_length  =
				extract_n2mbits(reg_value, 0, 3);
			entry_tid           = track_entry.ar_trans_tid[i];

			ptr += sprintf(ptr,
				"read entry = %d, valid = 0x%x, secure = 0x%x,",
				i, entry_valid, entry_secure);
			ptr += sprintf(ptr,
				"read id = 0x%x, address = 0x%x, data_size = 0x%x,",
				entry_id, entry_address, entry_data_size);
			ptr += sprintf(ptr, "burst_length = 0x%x\n",
					entry_burst_length);
		}

		/* BUS_DBG_AW_TRACK_L(__n)
		 * [31:0] AWADDR: DBG write tracker entry write address
		 */

		/* BUS_DBG_AW_TRACK_H(__n)
		 * [21] Valid:DBG   write tracker entry valid
		 * [20:8] ARID:DBG  write tracker entry write ID
		 * [6:4] ARSIZE:DBG write tracker entry write data size
		 * [3:0] ARLEN: DBG write tracker entry write burst length
		 */

		/* BUS_DBG_AW_TRACK_TID(__n)
		 * [2:0] BUS_DBG_AW_TRANS0_ENTRY_ID:
		 * DBG write tracker entry ID of 1st transaction
		 */

		for (i = 0; i < BUS_DBG_NUM_TRACKER; i++) {
			entry_address       = track_entry.aw_track_l[i];
			reg_value           = track_entry.aw_track_h[i];
			entry_valid         =
				extract_n2mbits(reg_value, 22, 22);
			entry_secure         =
				extract_n2mbits(reg_value, 21, 21);
			entry_id            =
				extract_n2mbits(reg_value, 8, 20);
			entry_data_size     =
				extract_n2mbits(reg_value, 4, 6);
			entry_burst_length  =
				extract_n2mbits(reg_value, 0, 3);
			entry_tid           = track_entry.aw_trans_tid[i];

			ptr += sprintf(ptr,
				"write entry = %d, valid = 0x%x, secure = 0x%x,",
				i, entry_valid, entry_secure);
			ptr += sprintf(ptr,
				"read id = 0x%x, address = 0x%x, data_size = 0x%x, ",
				entry_id, entry_address, entry_data_size);
			ptr += sprintf(ptr,
				"burst_length = 0x%x\n",
				entry_burst_length);
		}

		ptr += sprintf(ptr,
			"write entry ~ 6, valid = 0x%x, data = 0x%x\n",
			((track_entry.w_track_data_valid&(0x1<<6))>>6),
			track_entry.w_track_data6);
		ptr += sprintf(ptr,
			"write entry ~ 7, valid = 0x%x, data = 0x%x\n",
			((track_entry.w_track_data_valid&(0x1<<7))>>7),
			track_entry.w_track_data7);

		return strlen(buf);
	}

	return -1;
}

static ssize_t tracker_run_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
		"BUS_DBG_CON=0x%x, BUS_DBG_CON_INFRA=0x%x\n",
			readl(BUS_DBG_CON),
			readl(BUS_DBG_CON_INFRA));
}

static ssize_t tracker_run_store(struct device_driver *driver,
		const char *buf, size_t count)
{
	unsigned int value;

	if (kstrtou32(buf, 10, &value))
		return -EINVAL;

	if (value == 1)
		systracker_enable();
	else if (value == 0)
		systracker_disable();
	else
		return -EINVAL;

	return count;
}

DRIVER_ATTR_RW(tracker_run);

static ssize_t enable_wp_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%x\n", track_config.enable_wp);
}

static ssize_t enable_wp_store(struct device_driver *driver,
		const char *buf, size_t count)
{
	unsigned int value;

	if (kstrtou32(buf, 10, &value))
		return -EINVAL;

	if (value == 1)
		systracker_watchpoint_enable();
	else if (value == 0)
		systracker_watchpoint_disable();
	else
		return -EINVAL;

	return count;
}

static DRIVER_ATTR_RW(enable_wp);

static ssize_t set_wp_address_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%x\n", track_config.wp_phy_address);
}

int systracker_set_watchpoint_addr(unsigned int addr)
{
	if (mt_systracker_drv.set_watchpoint_address)
		return mt_systracker_drv.set_watchpoint_address(addr);

	track_config.wp_phy_address = addr;

	return 0;
}

static ssize_t set_wp_address_store
	(struct device_driver *driver, const char *buf, size_t count)
{
	unsigned int value;
	int ret;

	ret = kstrtou32(buf, 16, &value);
	pr_debug("watch address:0x%x, ret = %d\n", value, ret);
	systracker_set_watchpoint_addr(value);

	return count;
}

static DRIVER_ATTR_RW(set_wp_address);

static ssize_t tracker_entry_dump_show
	(struct device_driver *driver, char *buf)
{
	int ret = tracker_dump(buf);

	if (ret == -1)
		pr_notice("Dump error in %s, %d\n", __func__, __LINE__);

	return strlen(buf);
}


static ssize_t tracker_swtrst_show(struct device_driver *driver, char *buf)
{
	return 0;
}

static ssize_t tracker_swtrst_store
	(struct device_driver *driver, const char *buf, size_t count)
{
	writel(readl(BUS_DBG_CON) |
		BUS_DBG_CON_SW_RST, BUS_DBG_CON);
	return count;
}

static DRIVER_ATTR_RW(tracker_swtrst);

static ssize_t tracker_entry_dump_store
	(struct device_driver *driver, const char *buf, size_t count)
{
	return count;
}

static DRIVER_ATTR_RW(tracker_entry_dump);

static ssize_t tracker_last_status_show
	(struct device_driver *driver, char *buf)
{

	if (track_entry.dbg_con & BUS_DBG_CON_TIMEOUT)
		return snprintf(buf, PAGE_SIZE, "1\n");
	else
		return snprintf(buf, PAGE_SIZE, "0\n");
}

static ssize_t tracker_last_status_store
	(struct device_driver *driver, const char *buf, size_t count)
{
	return count;
}

static DRIVER_ATTR_RW(tracker_last_status);

/*
 * driver initialization entry point
 */
static int __init systracker_init(void)
{
	int err = 0;
	int ret = 0;

	err = platform_driver_register(&mt_systracker_drv.driver);
	if (err)
		return err;

	/* Create sysfs entry */
	ret  = driver_create_file(&mt_systracker_drv.driver.driver,
		&driver_attr_tracker_entry_dump);
	ret |= driver_create_file(&mt_systracker_drv.driver.driver,
		&driver_attr_tracker_run);
	ret |= driver_create_file(&mt_systracker_drv.driver.driver,
		&driver_attr_enable_wp);
	ret |= driver_create_file(&mt_systracker_drv.driver.driver,
		&driver_attr_set_wp_address);
	ret |= driver_create_file(&mt_systracker_drv.driver.driver,
		&driver_attr_tracker_swtrst);
	ret |= driver_create_file(&mt_systracker_drv.driver.driver,
		&driver_attr_tracker_last_status);
	if (ret)
		pr_notice("Fail to create systracker_drv sysfs files");

	pr_debug("systracker init done\n");

	return 0;
}

/*
 * driver exit point
 */
static void __exit systracker_exit(void)
{
}

arch_initcall_sync(systracker_init);
module_exit(systracker_exit);
